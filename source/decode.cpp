/*
    Generate native 1:1 world map for "You Have To Win The Game"
    Copyleft (C) 2022 Michaelangel007

    Reads:
        yhtwtg.map
        tiles_raw_indexed.data

    Writes:
        WorldMap1D_1x149_rooms_rgba32_320x28608.data
        WorldMap2D_19x13_rooms_rgba32_6080x2600.data
        tiles_32x32_rgba32_256x256.data
        WorldMap2D_19x13_rooms.bmp

NOTE: Solution > Properites > Debugging > Working directory should be set to:
    $(ProjectDir)data\
*/
// Uncomment to create different stages of the world map images
//#define TUTORIAL 3
// Uncomment to write: font_cga_rgba32_8x2048.data
//#define SAVE_CGA_FONT

#define _CRT_SECURE_NO_WARNINGS // MSVC bullshit

#include <stdio.h>  // printf(), fopen(), fclose()
#include <string.h> // memset()
#include <stdint.h> // uint8_t
#include <assert.h> // assert()

//#include <unistd.h> // Unix: getcwd()
#include <direct.h>   // Win: _getcwd()
#if _WIN32
    #define getcwd _getcwd
#endif

// Macros

	// NOTE: ROW_H = TILE_W_px * TILE_H_px * ATLAS_W_tiles
	#define GET_IMAGE_OFFSET(iTileX, iTileY, iTileW, iRowH) ((iTileY * iRowH) + (iTileX * iTileW))

// Consts

	const size_t K = 1024;

	// Texture Atlas
	const int TILE_W = 8; // in px
	const int TILE_H = 8; // in px
	const int TILE_Z = TILE_W * TILE_H;

	const int ATLAS_W = 32; // in tiles
	const int ATLAS_H = 32; // in tiles
	const int NUM_TILE = ATLAS_W * ATLAS_H;

	const int ATLAS_IMAGE_W = TILE_W * ATLAS_W;
	const int ATLAS_IMAGE_H = TILE_H * ATLAS_H;

	// We export TWO native 1:1 World maps:
	//
	//   1. An image with a single column of rooms: 1x149 , 320x(192*149) = 320 x 28,608 px
	//   2. An image with a 2D Grid 19x13 rooms with names, 19*320x13*200 = 6,080 x 2600 px

	// Rooms: 1C = single column of rooms, room sans room name
	const int ROOM1C_W    = 40; // in tiles
	const int ROOM1C_H    = 24; // in tiles
	const int ROOM1C_Z    = ROOM1C_W * ROOM1C_H; // NOTE: stored with 2 bytes/tile index

	const int ROOM1C_W_PX = TILE_W * ROOM1C_W;
	const int ROOM1C_H_PX = TILE_H * ROOM1C_H;

	// Rooms: 2D = grid of rooms, room with room name
	const int ROOM2D_W    = ROOM1C_W    ;		 // NOTE: No change in width, 320px
	const int ROOM2D_H    = ROOM1C_H + 1;        // For status line containing room name
	const int ROOM2D_W_PX = TILE_W   * ROOM2D_W; // px
	const int ROOM2D_H_PX = TILE_H   * ROOM2D_H; // px
	const int ROOM2D_Z    = ROOM2D_W * ROOM2D_H; // px

	const int MAX_ROOM    = 250; // Disk has ~150 rooms, max World Size = 19x13 = 247

	// 1. World Map 1x149 Rooms
	const int    MAP1C_ROOM_W  =   1; // rooms
	const int    MAP1C_ROOM_H  = 149;

	const int    MAP1C_IMAGE_W = ROOM1C_W_PX * MAP1C_ROOM_W; // px
	const int    MAP1C_IMAGE_H = ROOM1C_H_PX * MAP1C_ROOM_H;

	const int    ROOM1C_PIXELS =     MAP1C_IMAGE_W * ROOM1C_H_PX  ; // 320x192 (8*8 * 40*24) = 61440
	const size_t MAP1C_SIZE    = 4 * MAP1C_IMAGE_W * MAP1C_IMAGE_H; // 4 = RGBA channels

	// 2. World Map 19x13 Rooms
	const int    MAP2D_ROOW_W  = 19; // rooms
	const int    MAP2D_ROOM_H  = 13;

	const int    MAP2D_IMAGE_W = ROOM2D_W_PX * MAP2D_ROOW_W; // px
	const int    MAP2D_IMAGE_H = ROOM2D_H_PX * MAP2D_ROOM_H;

	const int    ROOM2D_PIXELS =     MAP2D_IMAGE_W * ROOM2D_H_PX  ; // 6080x200 (320*19 * 200) = 1216000
	const size_t MAP2D_SIZE    = 4 * MAP2D_IMAGE_W * MAP2D_IMAGE_H; // 4 = RGBA channels

	// Tiles
	//                           LE yyxx
	const uint16_t META_version = 0x0201;

	const uint16_t TILE_REspawn = 0x0114;
	const uint16_t TILE_reSPawn = 0x0115;
	const uint16_t TILE_respAwn = 0x0116;
	const uint16_t TILE_respaWN = 0x0117;

// Types

#pragma pack(push,2)
	struct MapHeader_t
	{
		int16_t nVersion;
		int32_t nPlayerStartRoomX;
		int32_t nPlayerStartRoomY;
		int32_t nUnknown1;
		int32_t nUnknown2;
		int32_t nRooms;
	};
#pragma pack(pop)
	static bool MAP_HEADER_IS_22_BYTES[ (sizeof(MapHeader_t) == 22) ];

	struct RoomDesc_t
	{
		int8_t      nRoomX; // World Map
		int8_t      nRoomY; // World Map
		const char *pDesc ;
		const char *pDesc2;
	};

	struct Room_t
	{
		int16_t*    pRoomData;
		RoomDesc_t* pRoomDesc;
		int         nRoomSize;
		int         nRoomId;
		int         nRoomX; // World Map
		int         nRoomY; // World Map
	};

	struct WorldMeta_t
	{
		int nMinRoomX;
		int nMinRoomY;
		int nMaxRoomX;
		int nMaxRoomY;
		int nMapWidth;
		int nMapHeight;
	};

// Globals

	// Map
	size_t  gSize = 0;
	uint8_t gRawMap[300 * K];

	MapHeader_t gMapHeader;
	WorldMeta_t gWorldMeta;

	// World Maps 1:1 Image
	uint32_t        gWorldMap1D[ MAP1C_SIZE ];
	uint32_t        gWorldMap2D[ MAP2D_SIZE ];

	// Room Descriptions
	// Source file: Rooms_Normal.xml
	RoomDesc_t gRoomDescriptions[] =
	{
		//           0         1         2         3
		//           0123456789012345678901234567890
		  {  99,99, "-- !!!Undocumented room!!! -- "    }

		, { -10, 2, "Slippery Slope"                    }
		, { -10, 3, "Nice of You to Drop In"            }

		, {  -9, 2, "Hollow King"                       }
		, {  -9, 3, "Foot of the Throne"                }
		, {  -9, 4, "Welcome to the Underground"        }
		, {  -9, 5, "Long Way Down"                     }
		, {  -9, 6, "Secret Passage"                    }

		, {  -8, 0, "-- OUT OF BOUNDS maps, legends --" } // Undocumented
		, {  -8, 1, "Maps and Legends"                  }
		, {  -8, 2, "Cerulean Aura"                     , " - Activates Blue Ghost Blocks -" }
		, {  -8, 3, "Mind the Gap"                      }
		, {  -8, 4, "Rock Transept"                     }
		, {  -8, 5, "Avalon Calling"                    }
		, {  -8, 6, "The Crab Cake Is a Lie"            }

		, {  -7, 0, "-- OUT OF BOUNDS yggdrasil --"     } // Undocumented
		, {  -7, 1, "Yggdrasil"                         }
		, {  -7, 2, "Covert Operators"                  }
		, {  -7, 3, "I Wonder Where This Goes"          }
		, {  -7, 4, "Aqueous Humor"                     }
		, {  -7, 5, "Et in Aether ego"                  }
		, {  -7, 6, "Be Seeing You"                     }

		, {  -6, 0, "-- OUT OF BOUNDS grand vault --"   } // Undocumented
		, {  -6, 1, "The Grand Vault"                   }
		, {  -6, 2, "The Quarry Hub"                    }
		, {  -6, 3, "Pit Stop"                          }
		, {  -6, 4, "Like Ivy, Twisting"                }
		, {  -6, 5, "It's More Scared of You"           }
		, {  -6, 6, "Hidden Crevasse"                   }

		, {  -5,-4, "-- OUT OF BOUNDS silver moon --"   } // Undocumented
		, {  -5,-3, "A Sickly Silver Moon"              }
		, {  -5,-2, "Before the Crash"                  }
		, {  -5,-1, "Hold On Tight and Don't Look Down" }
		, {  -5, 0, "Hydra Is Myth"                     }
		, {  -5, 1, "Artisan Stone Walls"               }
		, {  -5, 2, "Cave In"                           }
		, {  -5, 3, "Obvious Movie Quote"               }
		, {  -5, 4, "A Dream of a Memory"               }
		, {  -5, 5, "Mine Shaft"                        }
		, {  -5, 6, "Forgotten Tunnels"                 }

		, {  -4,-4, "-- OUT OF BOUNDS used to live --"  } // Undocumented
		, {  -4,-3, "This Is Where We Used to Live"     }
		, {  -4,-2, "A Brief Respite"                   }
		, {  -4,-1, "The Coin and the Courage"          }
		, {  -4, 0, "Arcane Vocabulary"                 }
		, {  -4, 1, "Venn's Banality"                   }
		, {  -4, 2, "Crawlspace"                        }
		, {  -4, 3, "Don't Be Hasty"                    }
		, {  -4, 4, "A Memory of a Dream"               }
		, {  -4, 5, "Green Man"                         }
		, {  -4, 6, "Descent"                           }

		, {  -3,-4, "-- OUT OF BOUNDS catastrophe --"   } // Undocumented
		, {  -3,-3, "Catharsis in Catastrophe"          }
		, {  -3,-2, "Hardcore Prawn"                    }
		, {  -3,-1, "Exit Strategy"                     }
		, {  -3, 0, "You Have to Start the Game"        }
		, {  -3, 1, "Hops and Skips"                    }
		, {  -3, 2, "Never Could See Any Other Way"     }
		, {  -3, 3, "You Definitely Shouldn't Go Left"  }
		, {  -3, 4, "Rawr!"                             }
		, {  -3, 5, "Springheel Boots"                  , " - Jump Again in Midair -" }
		, {  -3, 6, "The Arbitrarium"                   }

		, {  -2,-3, "Linchpin"                          }
		, {  -2,-2, "Point of No Return"                }
		, {  -2,-1, "Playing with Fire"                 }
		, {  -2, 0, "KISS Principle"                    }
		, {  -2, 1, "Leaps and Bounds"                  }
		, {  -2, 2, "Pit of Spikes"                     }
		, {  -2, 3, "Tower of Sorrows"                  }
		, {  -2, 4, "Tower of Regrets"                  }
		, {  -2, 5, "Back to the Surface"               }

		, {  -1,-4, "-- OUT OF BOUNDS Warp Left --"     } // Undocumented
		, {  -1,-3, "Speak Now..."                      }
		, {  -1,-2, "Open Sesame"                       }
		, {  -1,-1, "From Another World"                }
		, {  -1, 0, "Snake, It's a Snake"               }
		, {  -1, 1, "Swimming Upstream"                 }
		, {  -1, 2, "Subterranea"                       }
		, {  -1, 3, "Contrived Lock/Key Mechanisms"     }
		, {  -1, 4, "Falling Into a Greener Life"       }

		, {   0,-4, "-- OUT OF BOUNDS Warp Middle --"   } // Undocumented
		, {   0,-3, "Consolation Prize"                 }
		, {   0,-2, "Eponymous"                         }
		, {   0,-1, "Harbinger"                         }
		, {   0, 0, "Treasure Hunt"                     }
		, {   0, 1, "Abstract Bridge"                   }
		, {   0, 2, "Which Path Will I Take?"           }
		, {   0, 3, "Bat Cave"                          }
		, {   0, 4, "Euclid Shrugged"                   }

		, {   1,-4, "-- OUT OF BOUNDS Warp Right --"    } // Undocumented
		, {   1,-1, "Taking the Long Way"               }
		, {   1, 0, "Danger"                            }
		, {   1, 1, "Not All Those Who Wander Are Lost" }
		, {   1, 2, "Cognitive Resonance"               }
		, {   1, 3, "Functional Spelæology"             }
		, {   1, 4, "Circular Logic"                    }

		, {   2, 0, "Leap of Faith"                     }
		, {   2, 1, "Stick the Landing"                 }
		, {   2, 2, "Precarious Footholds"              }
		, {   2, 3, "Fungal Forest"                     }
		, {   2, 4, "Prawn Shot First"                  }

		, {   3,-4, "Spiral Out"                        } // Secret, Trivia: Reference to Tool's 2001 Album "Lateralus" ?
		, {   3,-3, "Keep Going"                        } // Secret, Trivia: Reference to Tool's 2001 Album "Lateralus" ?
		, {   3, 0, "On the Count of Three"             }
		, {   3, 1, "Mushroom Staircase"                }
		, {   3, 2, "Transplants"                       }
		, {   3, 3, "The Proper Motivation"             }
		, {   3, 4, "Fish Out of Water"                 }
		, {   3, 5, "Abandoned Alcove"                  }
		, {   3, 6, "yeah but why u jelly tho"          }
		, {   3, 7, "-- OUT OF BOUNDS jelly tho -- "    } // Undocumented

		, {   4,-5, "The Books Will Not"                } // Secret
		, {   4,-4, "Know Our Names"                    } // Secret
		, {   4,-3, "Are You Watching Closely?"         } // Secret
		, {   4, 0, "Does Whatever A Spider Does"       }
		, {   4, 1, "Eden Maw"                          }
		, {   4, 2, "Under Construction"                }
		, {   4, 3, "Remnants of a Past Unknown"        }
		, {   4, 4, "Castle Rock"                       }
		, {   4, 5, "Observation Deck"                  }
		, {   4, 6, "Wellspring"                        }

		, {   5,-3, "The Solo From Oaks"                } // Secret
		, {   5,-2, "No Fun Without the Danger"         } // Secret
		, {   5, 0, "Attic Storeroom"                   }
		, {   5, 1, "Vestibule"                         }
		, {   5, 2, "Shelter from the Storm"            }
		, {   5, 3, "Ghosts"                            }
		, {   5, 4, "Uncertain Semiotics"               }
		, {   5, 5, "Don't Get Snippy With Me"          }
		, {   5, 6, "Crimson Aura"                      , "- Activates Red Ghost Blocks -"}

		, {   6,-3, "Loaded Dice"                       } // Secret
		, {   6,-0, "Clarity Comes in Waves"            }
		, {   6, 1, "Great Hall"                        }
		, {   6, 2, "Cave Painting"                     }
		, {   6, 3, "The Loneliest Corner"              }
		, {   6, 4, "Rough Landing"                     }
		, {   6, 5, "Bring a Mallet"                    }
		, {   6, 6, "Dire Crab"                         }

		, {   7,-3, "Re: Volver"                        } // Secret
		, {   7,-1, "An Even 0x80"                      }
		, {   7,-0, "The Floor Is Lava"                 }
		, {   7, 1, "Hollow King Transformed"           }
		, {   7, 2, "Worth It?"                         }
		, {   7, 3, "Secret Cat Level"                  }
		, {   7, 4, "Feline Foreshadowing"              }

		, {   8,-3, "Melancholy"                        } // Secret
		, {   8,-2, "Sadness"                           } // Secret
		, {   8, 0, "Brazen Machines"                   }
		, {   8, 1, "Spider Gloves"                     , "'- Cling to Walls and Leap Off -"}
		, {   8, 2, "Not Worth It!"                     }
	};
	const size_t NUM_ROOM_DESCRIPTIONS = sizeof(gRoomDescriptions) / sizeof(gRoomDescriptions[0]);

	// Room Pointers
	Room_t   gRooms[ MAX_ROOM ];

	size_t   nTilesRawIndex = 0;
	uint8_t  gTilesRawIndex[TILE_Z * NUM_TILE * 3]; // 3 bytes/pixel (RGB)
	uint32_t gTilesRGBA    [TILE_Z * NUM_TILE    ]; // 4 bytes/pixel (RGBA)

	// CGA Colorss
	uint32_t gPalette[16] =
	{
		//  aabbggrr
		  0xFF000000 // 0 Black
		, 0xFFaa0000 // 1 Blue
		, 0xFF00aa00 // 2 Green
		, 0xFFaaaa00 // 3 Cyan
		, 0xFF0000aa // 4 Red
		, 0xFFaa00aa // 5 Magenta
		, 0xFF0055aa // 6 Brown
		, 0xFFaaaaaa // 7 Light Grey
		, 0xFF555555 // 8 Dark Grey
		, 0xFFff5555 // 9 Light Blue
		, 0xFF55ff55 // A Light Green
		, 0xFFffff55 // B Light Cyan
		, 0xFF5555ff // C Light Red
		, 0xFFff55ff // D Light Magenta
		, 0xFF55ffff // E Yellow
		, 0xFFffffff // F White
	};

	const int CGA_TILE_W     =  8; // px
	const int CGA_TILE_H     =  8;
	const int CGA_TILE_Z     = CGA_TILE_W * CGA_TILE_H;

	const int CGA_ATLAS_W    = 32; // tiles
	const int CGA_ATLAS_H    =  8;
	const int CGA_ATLAS_Z    = CGA_ATLAS_W * CGA_ATLAS_H; // Total glyphs

	const int CGA_ROW_W      = CGA_TILE_W * CGA_ATLAS_W; // px
	const int CGA_ROW_H      = CGA_TILE_Z * CGA_ATLAS_W; // px

	const int CGA_ATLAS_2D_Z = CGA_TILE_Z * CGA_ATLAS_Z; // px
	uint32_t gRawCGAFont[ CGA_ATLAS_2D_Z ]; // raw 2D 256x64, 32-bpp

	/*
	// NOTE: IBM had multiple revisions for their CGA font
	//
	// Verion 1 has numerous bad glyphs
	//     04 Diamonds (Tail !?)
	//     05 Clubs    (Stem is too fat)
	//     06 Spades   (Stem is too fat)
	//     53 S        (Extra Blocky)
	//
	// Version 2 fix the Diamonds, Clubs, Spaces, and S but mangled the White Sun glyph:
	//     0F ☼ U+263C White Sun with Rays
	//
	//     01234567       01234567        01234567       01234567        01234567       01234567  
	//    +--------+     +--------+      +--------+     +--------+      +--------+     +--------+ 
	//   0|   X    |0   0|   X    |0    0|  XXX   |0   0|  XXX   |0    0|   X    |0   0|   X    |0
	//   1|  XXX   |1   1|  XXX   |1    1| XXXXX  |1   1| XXXXX  |1    1|   X    |1   1|   X    |1
	//   2| XXXXX  |2   2| XXXXX  |2    2|  XXX   |2   2|  XXX   |2    2|  XXX   |2   2|  XXX   |2
	//   3|XXXXXXX |3   3|XXXXXXX |3    3|XXXXXXX |3   3|XXXXXXX |3    3| XXXXX  |3   3| XXXXX  |3
	//   4| XXXXX  |4   4| XXXXX  |4    4|XXXXXXX |4   4|XXXXXXX |4    4|XXXXXXX |4   4|XXXXXXX |4
	//   5|  XXX   |5   5|  XXX   |5    5| XXXXX  |5   5| X X X  |5    5| XXXXX  |5   5| XXXXX  |5
	//   6|   X    |6   6|   X    |6    6|  XXX   |6   6|   X    |6    6|  XXX   |6   6|   X    |6
	//   7|    X   |7   7|        |7    7| XXXXX  |7   7|  XXX   |7    7| XXXXX  |7   7|  XXX   |7
	//    +--------+     +--------+      +--------+     +--------+      +--------+     +--------+ 
	//     01234567      01234567         01234567      01234567         01234567      01234567   
	//     Ver 1         Ver 2            Ver 1         Ver 2            Ver 1         Ver 2      
	//
	//     01234567       01234567        01234567       01234567  
	//    +--------+     +--------+      +--------+     +--------+ 
	//   0| XXXX   |0   0| XXXX   |0    0|X  XX  X|0   0|   XX   |0
	//   1|XX  XX  |1   1|XX  XX  |1    1| X XX X |1   1|XX XX XX|1
	//   2|XXX     |2   2| XX     |2    2|  XXXX  |2   2|  XXXX  |2
	//   3| XXX    |3   3|  XX    |3    3|XXX  XXX|3   3|XXX  XXX|3
	//   4|   XXX  |4   4|   XX   |4    4|XXX  XXX|4   4|XXX  XXX|4
	//   5|XX  XX  |5   5|XX  XX  |5    5|  XXXX  |5   5|  XXXX  |5
	//   6| XXXX   |6   6| XXXX   |6    6| X XX X |6   6| X XX X |6
	//   7|        |7   7|        |7    7|X  XX  X|7   7|X  XX  X|7
	//    +--------+     +--------+      +--------+     +--------+ 
	//     01234567      01234567         01234567      01234567   
	//     Ver 1         Ver 2            Ver 1         Ver 2      
	//
	//   All versions have bad glyphs
	//     62 b
	//     64 d
	//   Tandy fixed the `b` and `d` in their font.
	//
	// See:
	//   http://nerdlypleasures.blogspot.com/2015/04/ibm-character-fonts.html
	//  CGA 1 http://1.bp.blogspot.com/-mt8oWJ49rew/VSHWWsZ4rZI/AAAAAAAACpY/7jSIij-kaV0/s1600/pc0-8x8.png
	//  CGA 2 http://2.bp.blogspot.com/-mO2qndnI8NI/VSHWTipp1hI/AAAAAAAACoQ/no-CMfAT5Jc/s1600/cga8x8b.png
	//  Tandy http://4.bp.blogspot.com/-wEQ2dcm-TuE/VSHWXMMcJdI/AAAAAAAACpk/H3NaqJKGexE/s1600/t1k-8x8-437.png
	*/
	uint8_t gPackedFont8x8RGBA[ CGA_ATLAS_Z * 8 ] = // 1D, 256 glyphs * 1x8, 1-bpp
	{   // https://int10h.org/oldschool-pc-fonts/fontlist/font?ibm_bios
		// https://en.wikipedia.org/wiki/Code_page_437#Character_set
		// https://upload.wikimedia.org/wikipedia/commons/f/f8/Codepage-437.png
		//  0    1    2    3    4    5    6    7
		 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00 // 00   U+0000 NUL
		,0x7E,0x81,0xA5,0x81,0xBD,0x99,0x81,0x7E // 01 ☺ U+263A White Smiling Face           (Alt  1)
		,0x7E,0xFF,0xDB,0xFF,0xC3,0xE7,0xFF,0x7E // 02 ☻ U+263B Black Smiling Face           (Alt  2)
		,0x6C,0xFE,0xFE,0xFE,0x7C,0x38,0x10,0x00 // 03 ♥ U+2665 Hearts                       (Alt  3)
		,0x10,0x38,0x7C,0xFE,0x7C,0x38,0x10,0x00 // 04 ♦ U+2666 Diamonds                     (Alt  4)
		,0x38,0x7C,0x38,0xFE,0xFE,0xD6,0x10,0x38 // 05 ♣ U+2663 Clubs                        (Alt  5)
		,0x10,0x10,0x38,0x7C,0xFE,0x7C,0x10,0x38 // 06 ♠ U+2660 Spades                       (Alt  6)
		,0x00,0x00,0x18,0x3C,0x3C,0x18,0x00,0x00 // 07 • U+2022 Bullet                       (Alt  7)
		,0xFF,0xFF,0xE7,0xC3,0xC3,0xE7,0xFF,0xFF // 08 ◘ U+25D8 Inverse Bullet               (Alt  8)
		,0x00,0x3C,0x66,0x42,0x42,0x66,0x3C,0x00 // 09 ○ U+25CB White Circle                 (Alt  9)
		,0xFF,0xC3,0x99,0xBD,0xBD,0x99,0xC3,0xFF // 0A ◙ U+25D9 Inverse White Circle         (Alt 10)
		,0x0F,0x07,0x0F,0x7D,0xCC,0xCC,0xCC,0x78 // 0B ♂ U+2642 Male Sign                    (Alt 11)
		,0x3C,0x66,0x66,0x66,0x3C,0x18,0x7E,0x18 // 0C ♀ U+2640 Female Sign                  (Alt 12)
		,0x3F,0x33,0x3F,0x30,0x30,0x70,0xF0,0xE0 // 0D ♪ U+266A Eigth Note                   (Alt 13)
		,0x7F,0x63,0x7F,0x63,0x63,0x67,0xE6,0xC0 // 0E ♫ U+266B Sixteen Note                 (Alt 14)
		,0x99,0x5A,0x3C,0xE7,0xE7,0x3C,0x5A,0x99 // 0F ☼ U+263C White Sun With Rays          (Alt 15) Revised: 0x18,0xDB,0x3C,0xE7,0xE7,0x3C,0xDB,0x18
		,0x80,0xE0,0xF8,0xFE,0xF8,0xE0,0x80,0x00 // 10 ► U+25BA Black Right Pointing Pointer (Alt 16)
		,0x02,0x0E,0x3E,0xFE,0x3E,0x0E,0x02,0x00 // 11 ◄ U+25C4 Black Left Pointint Pointer  (Alt 17)
		,0x18,0x3C,0x7E,0x18,0x18,0x7E,0x3C,0x18 // 12 ↕ U+2195 Up Down Arrow                (Alt 18)
		,0x66,0x66,0x66,0x66,0x66,0x00,0x66,0x00 // 13 ‼ U+203C Double Exclamaition Mark     (Alt 19)
		,0x7F,0xDB,0xDB,0x7B,0x1B,0x1B,0x1B,0x00 // 14 ¶ U+00B6 Pilcrow                      (Alt 20)
		,0x3E,0x63,0x38,0x6C,0x6C,0x38,0xCC,0x78 // 15 § U+00A7 Section Sign                 (Alt 21)
		,0x00,0x00,0x00,0x00,0x7E,0x7E,0x7E,0x00 // 16 ▬ U+25AC Black Rectangle              (Alt 22)
		,0x18,0x3C,0x7E,0x18,0x7E,0x3C,0x18,0xFF // 17 ↨ U+21A8 Up Down Arrow with Base      (Alt 23)
		,0x18,0x3C,0x7E,0x18,0x18,0x18,0x18,0x00 // 18 ↑ U+2191 Up Arrow                     (Alt 24)
		,0x18,0x18,0x18,0x18,0x7E,0x3C,0x18,0x00 // 19 ↓ U+2193 Down Arrow                   (Alt 25)
		,0x00,0x18,0x0C,0xFE,0x0C,0x18,0x00,0x00 // 1A → U+2192 Right Arrow                  (Alt 26)
		,0x00,0x30,0x60,0xFE,0x60,0x30,0x00,0x00 // 1B ← U+2190 Left Arrow                   (Alt 27)
		,0x00,0x00,0xC0,0xC0,0xC0,0xFE,0x00,0x00 // 1C ∟ U+221F Right Angle                  (Alt 28)
		,0x00,0x24,0x66,0xFF,0x66,0x24,0x00,0x00 // 1D ↔ U+2194 Left Right Arrow             (Alt 29)
		,0x00,0x18,0x3C,0x7E,0xFF,0xFF,0x00,0x00 // 1E ▲ U+25B2 Black Up Pointing Triangle   (Alt 30)
		,0x00,0xFF,0xFF,0x7E,0x3C,0x18,0x00,0x00 // 1F ▼ U+25BC Black Down Pointing Triangle (Alt 31)
		,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00 // 20   U+0020
		,0x30,0x78,0x78,0x30,0x30,0x00,0x30,0x00 // 21 ! U+0021
		,0x6C,0x6C,0x6C,0x00,0x00,0x00,0x00,0x00 // 22 " U+0022
		,0x6C,0x6C,0xFE,0x6C,0xFE,0x6C,0x6C,0x00 // 23 #
		,0x30,0x7C,0xC0,0x78,0x0C,0xF8,0x30,0x00 // 24 $
		,0x00,0xC6,0xCC,0x18,0x30,0x66,0xC6,0x00 // 25 %
		,0x38,0x6C,0x38,0x76,0xDC,0xCC,0x76,0x00 // 26 &
		,0x60,0x60,0xC0,0x00,0x00,0x00,0x00,0x00 // 27 '
		,0x18,0x30,0x60,0x60,0x60,0x30,0x18,0x00 // 28 (
		,0x60,0x30,0x18,0x18,0x18,0x30,0x60,0x00 // 29 )
		,0x00,0x66,0x3C,0xFF,0x3C,0x66,0x00,0x00 // 2A *
		,0x00,0x30,0x30,0xFC,0x30,0x30,0x00,0x00 // 2B +
		,0x00,0x00,0x00,0x00,0x00,0x30,0x30,0x60 // 2C ,
		,0x00,0x00,0x00,0xFC,0x00,0x00,0x00,0x00 // 2D -
		,0x00,0x00,0x00,0x00,0x00,0x30,0x30,0x00 // 2E .
		,0x06,0x0C,0x18,0x30,0x60,0xC0,0x80,0x00 // 2F /
		,0x7C,0xC6,0xCE,0xDE,0xF6,0xE6,0x7C,0x00 // 30 0
		,0x30,0x70,0x30,0x30,0x30,0x30,0xFC,0x00 // 31 1
		,0x78,0xCC,0x0C,0x38,0x60,0xCC,0xFC,0x00 // 32 2
		,0x78,0xCC,0x0C,0x38,0x0C,0xCC,0x78,0x00 // 33 3
		,0x1C,0x3C,0x6C,0xCC,0xFE,0x0C,0x1E,0x00 // 34 4
		,0xFC,0xC0,0xF8,0x0C,0x0C,0xCC,0x78,0x00 // 35 5
		,0x38,0x60,0xC0,0xF8,0xCC,0xCC,0x78,0x00 // 36 6
		,0xFC,0xCC,0x0C,0x18,0x30,0x30,0x30,0x00 // 37 7
		,0x78,0xCC,0xCC,0x78,0xCC,0xCC,0x78,0x00 // 38 8
		,0x78,0xCC,0xCC,0x7C,0x0C,0x18,0x70,0x00 // 39 9
		,0x00,0x30,0x30,0x00,0x00,0x30,0x30,0x00 // 3A :
		,0x00,0x30,0x30,0x00,0x00,0x30,0x30,0x60 // 3B ;
		,0x18,0x30,0x60,0xC0,0x60,0x30,0x18,0x00 // 3C <
		,0x00,0x00,0xFC,0x00,0x00,0xFC,0x00,0x00 // 3D =
		,0x60,0x30,0x18,0x0C,0x18,0x30,0x60,0x00 // 3E >
		,0x78,0xCC,0x0C,0x18,0x30,0x00,0x30,0x00 // 3F ?
		,0x7C,0xC6,0xDE,0xDE,0xDE,0xC0,0x78,0x00 // 40 @
		,0x30,0x78,0xCC,0xCC,0xFC,0xCC,0xCC,0x00 // 41 A
		,0xFC,0x66,0x66,0x7C,0x66,0x66,0xFC,0x00 // 42 B
		,0x3C,0x66,0xC0,0xC0,0xC0,0x66,0x3C,0x00 // 43 C
		,0xF8,0x6C,0x66,0x66,0x66,0x6C,0xF8,0x00 // 44 D
		,0xFE,0x62,0x68,0x78,0x68,0x62,0xFE,0x00 // 45 E
		,0xFE,0x62,0x68,0x78,0x68,0x60,0xF0,0x00 // 46 F
		,0x3C,0x66,0xC0,0xC0,0xCE,0x66,0x3E,0x00 // 47 G
		,0xCC,0xCC,0xCC,0xFC,0xCC,0xCC,0xCC,0x00 // 48 H
		,0x78,0x30,0x30,0x30,0x30,0x30,0x78,0x00 // 49 I
		,0x1E,0x0C,0x0C,0x0C,0xCC,0xCC,0x78,0x00 // 4A J
		,0xE6,0x66,0x6C,0x78,0x6C,0x66,0xE6,0x00 // 4B K
		,0xF0,0x60,0x60,0x60,0x62,0x66,0xFE,0x00 // 4C L
		,0xC6,0xEE,0xFE,0xFE,0xD6,0xC6,0xC6,0x00 // 4D M
		,0xC6,0xE6,0xF6,0xDE,0xCE,0xC6,0xC6,0x00 // 4E N
		,0x38,0x6C,0xC6,0xC6,0xC6,0x6C,0x38,0x00 // 4F O
		,0xFC,0x66,0x66,0x7C,0x60,0x60,0xF0,0x00 // 50 P
		,0x78,0xCC,0xCC,0xCC,0xDC,0x78,0x1C,0x00 // 51 Q
		,0xFC,0x66,0x66,0x7C,0x6C,0x66,0xE6,0x00 // 52 R
		,0x78,0xCC,0x60,0x30,0x18,0xCC,0x78,0x00 // 53 S
		,0xFC,0xB4,0x30,0x30,0x30,0x30,0x78,0x00 // 54 T
		,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xFC,0x00 // 55 U
		,0xCC,0xCC,0xCC,0xCC,0xCC,0x78,0x30,0x00 // 56 V
		,0xC6,0xC6,0xC6,0xD6,0xFE,0xEE,0xC6,0x00 // 57 W
		,0xC6,0xC6,0x6C,0x38,0x38,0x6C,0xC6,0x00 // 58 X
		,0xCC,0xCC,0xCC,0x78,0x30,0x30,0x78,0x00 // 59 Y
		,0xFE,0xC6,0x8C,0x18,0x32,0x66,0xFE,0x00 // 5A Z
		,0x78,0x60,0x60,0x60,0x60,0x60,0x78,0x00 // 5B [
		,0xC0,0x60,0x30,0x18,0x0C,0x06,0x02,0x00 // 5C \ 
		,0x78,0x18,0x18,0x18,0x18,0x18,0x78,0x00 // 5D ]
		,0x10,0x38,0x6C,0xC6,0x00,0x00,0x00,0x00 // 5E ^
		,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF // 5F _
		,0x30,0x30,0x18,0x00,0x00,0x00,0x00,0x00 // 60 `
		,0x00,0x00,0x78,0x0C,0x7C,0xCC,0x76,0x00 // 61 a
		,0xE0,0x60,0x60,0x7C,0x66,0x66,0xDC,0x00 // 62 b
		,0x00,0x00,0x78,0xCC,0xC0,0xCC,0x78,0x00 // 63 c
		,0x1C,0x0C,0x0C,0x7C,0xCC,0xCC,0x76,0x00 // 64 d
		,0x00,0x00,0x78,0xCC,0xFC,0xC0,0x78,0x00 // 65 e
		,0x38,0x6C,0x60,0xF0,0x60,0x60,0xF0,0x00 // 66 f
		,0x00,0x00,0x76,0xCC,0xCC,0x7C,0x0C,0xF8 // 67 g
		,0xE0,0x60,0x6C,0x76,0x66,0x66,0xE6,0x00 // 68 h
		,0x30,0x00,0x70,0x30,0x30,0x30,0x78,0x00 // 69 i
		,0x0C,0x00,0x0C,0x0C,0x0C,0xCC,0xCC,0x78 // 6A j
		,0xE0,0x60,0x66,0x6C,0x78,0x6C,0xE6,0x00 // 6B k
		,0x70,0x30,0x30,0x30,0x30,0x30,0x78,0x00 // 6C l
		,0x00,0x00,0xCC,0xFE,0xFE,0xD6,0xC6,0x00 // 6D m
		,0x00,0x00,0xF8,0xCC,0xCC,0xCC,0xCC,0x00 // 6E n
		,0x00,0x00,0x78,0xCC,0xCC,0xCC,0x78,0x00 // 6F o
		,0x00,0x00,0xDC,0x66,0x66,0x7C,0x60,0xF0 // 70 p
		,0x00,0x00,0x76,0xCC,0xCC,0x7C,0x0C,0x1E // 71 q
		,0x00,0x00,0xDC,0x76,0x66,0x60,0xF0,0x00 // 72 r
		,0x00,0x00,0x7C,0xC0,0x78,0x0C,0xF8,0x00 // 73 s
		,0x10,0x30,0x7C,0x30,0x30,0x34,0x18,0x00 // 74 t
		,0x00,0x00,0xCC,0xCC,0xCC,0xCC,0x76,0x00 // 75 u
		,0x00,0x00,0xCC,0xCC,0xCC,0x78,0x30,0x00 // 76 v
		,0x00,0x00,0xC6,0xD6,0xFE,0xFE,0x6C,0x00 // 77 w
		,0x00,0x00,0xC6,0x6C,0x38,0x6C,0xC6,0x00 // 78 x
		,0x00,0x00,0xCC,0xCC,0xCC,0x7C,0x0C,0xF8 // 79 y
		,0x00,0x00,0xFC,0x98,0x30,0x64,0xFC,0x00 // 7A z
		,0x1C,0x30,0x30,0xE0,0x30,0x30,0x1C,0x00 // 7B {
		,0x18,0x18,0x18,0x00,0x18,0x18,0x18,0x00 // 7C |
		,0xE0,0x30,0x30,0x1C,0x30,0x30,0xE0,0x00 // 7D }
		,0x76,0xDC,0x00,0x00,0x00,0x00,0x00,0x00 // 7E ~
		,0x00,0x10,0x38,0x6C,0xC6,0xC6,0xFE,0x00 // 7F ⌂ U+2302 House
		,0x78,0xCC,0xC0,0xCC,0x78,0x18,0x0C,0x78 // 80 Ç
		,0x00,0xCC,0x00,0xCC,0xCC,0xCC,0x7E,0x00 // 81 ü
		,0x1C,0x00,0x78,0xCC,0xFC,0xC0,0x78,0x00 // 82 é
		,0x7E,0xC3,0x3C,0x06,0x3E,0x66,0x3F,0x00 // 83 â
		,0xCC,0x00,0x78,0x0C,0x7C,0xCC,0x7E,0x00 // 84 ä
		,0xE0,0x00,0x78,0x0C,0x7C,0xCC,0x7E,0x00 // 85 à
		,0x30,0x30,0x78,0x0C,0x7C,0xCC,0x7E,0x00 // 86 å
		,0x00,0x00,0x78,0xC0,0xC0,0x78,0x0C,0x38 // 87 ç
		,0x7E,0xC3,0x3C,0x66,0x7E,0x60,0x3C,0x00 // 88 ê
		,0xCC,0x00,0x78,0xCC,0xFC,0xC0,0x78,0x00 // 89 ë
		,0xE0,0x00,0x78,0xCC,0xFC,0xC0,0x78,0x00 // 8A è
		,0xCC,0x00,0x70,0x30,0x30,0x30,0x78,0x00 // 8B ï
		,0x7C,0xC6,0x38,0x18,0x18,0x18,0x3C,0x00 // 8C î
		,0xE0,0x00,0x70,0x30,0x30,0x30,0x78,0x00 // 8D ì
		,0xC6,0x38,0x6C,0xC6,0xFE,0xC6,0xC6,0x00 // 8E Ä
		,0x30,0x30,0x00,0x78,0xCC,0xFC,0xCC,0x00 // 8F Å
		,0x1C,0x00,0xFC,0x60,0x78,0x60,0xFC,0x00 // 90 É
		,0x00,0x00,0x7F,0x0C,0x7F,0xCC,0x7F,0x00 // 91 æ
		,0x3E,0x6C,0xCC,0xFE,0xCC,0xCC,0xCE,0x00 // 92 Æ
		,0x78,0xCC,0x00,0x78,0xCC,0xCC,0x78,0x00 // 93 ô
		,0x00,0xCC,0x00,0x78,0xCC,0xCC,0x78,0x00 // 94 ö
		,0x00,0xE0,0x00,0x78,0xCC,0xCC,0x78,0x00 // 95 ò
		,0x78,0xCC,0x00,0xCC,0xCC,0xCC,0x7E,0x00 // 96 û
		,0x00,0xE0,0x00,0xCC,0xCC,0xCC,0x7E,0x00 // 97 ù
		,0x00,0xCC,0x00,0xCC,0xCC,0x7C,0x0C,0xF8 // 98 ÿ
		,0xC3,0x18,0x3C,0x66,0x66,0x3C,0x18,0x00 // 99 Ö
		,0xCC,0x00,0xCC,0xCC,0xCC,0xCC,0x78,0x00 // 9A Ü
		,0x18,0x18,0x7E,0xC0,0xC0,0x7E,0x18,0x18 // 9B ¢
		,0x38,0x6C,0x64,0xF0,0x60,0xE6,0xFC,0x00 // 9C £
		,0xCC,0xCC,0x78,0xFC,0x30,0xFC,0x30,0x30 // 9D ¥
		,0xF8,0xCC,0xCC,0xFA,0xC6,0xCF,0xC6,0xC7 // 9E ₧
		,0x0E,0x1B,0x18,0x3C,0x18,0x18,0xD8,0x70 // 9F ƒ
		,0x1C,0x00,0x78,0x0C,0x7C,0xCC,0x7E,0x00 // A0 á
		,0x38,0x00,0x70,0x30,0x30,0x30,0x78,0x00 // A1 í
		,0x00,0x1C,0x00,0x78,0xCC,0xCC,0x78,0x00 // A2 ó
		,0x00,0x1C,0x00,0xCC,0xCC,0xCC,0x7E,0x00 // A3 ú
		,0x00,0xF8,0x00,0xF8,0xCC,0xCC,0xCC,0x00 // A4 ñ
		,0xFC,0x00,0xCC,0xEC,0xFC,0xDC,0xCC,0x00 // A5 Ñ
		,0x3C,0x6C,0x6C,0x3E,0x00,0x7E,0x00,0x00 // A6 ª
		,0x38,0x6C,0x6C,0x38,0x00,0x7C,0x00,0x00 // A7 º
		,0x30,0x00,0x30,0x60,0xC0,0xCC,0x78,0x00 // A8 ¿
		,0x00,0x00,0x00,0xFC,0xC0,0xC0,0x00,0x00 // A9 ⌐
		,0x00,0x00,0x00,0xFC,0x0C,0x0C,0x00,0x00 // AA ¬
		,0xC3,0xC6,0xCC,0xDE,0x33,0x66,0xCC,0x0F // AB ½
		,0xC3,0xC6,0xCC,0xDB,0x37,0x6F,0xCF,0x03 // AC ¼
		,0x18,0x18,0x00,0x18,0x18,0x18,0x18,0x00 // AD ¡
		,0x00,0x33,0x66,0xCC,0x66,0x33,0x00,0x00 // AE «
		,0x00,0xCC,0x66,0x33,0x66,0xCC,0x00,0x00 // AF »
		,0x22,0x88,0x22,0x88,0x22,0x88,0x22,0x88 // B0 ░
		,0x55,0xAA,0x55,0xAA,0x55,0xAA,0x55,0xAA // B1 ▒
		,0xDB,0x77,0xDB,0xEE,0xDB,0x77,0xDB,0xEE // B2 ▓
		,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18 // B3 │
		,0x18,0x18,0x18,0x18,0xF8,0x18,0x18,0x18 // B4 ┤
		,0x18,0x18,0xF8,0x18,0xF8,0x18,0x18,0x18 // B5 ╡
		,0x36,0x36,0x36,0x36,0xF6,0x36,0x36,0x36 // B6 ╢
		,0x00,0x00,0x00,0x00,0xFE,0x36,0x36,0x36 // B7 ╖
		,0x00,0x00,0xF8,0x18,0xF8,0x18,0x18,0x18 // B8 ╕
		,0x36,0x36,0xF6,0x06,0xF6,0x36,0x36,0x36 // B9 ╣
		,0x36,0x36,0x36,0x36,0x36,0x36,0x36,0x36 // BA ║
		,0x00,0x00,0xFE,0x06,0xF6,0x36,0x36,0x36 // BB ╗
		,0x36,0x36,0xF6,0x06,0xFE,0x00,0x00,0x00 // BC ╝
		,0x36,0x36,0x36,0x36,0xFE,0x00,0x00,0x00 // BD ╜
		,0x18,0x18,0xF8,0x18,0xF8,0x00,0x00,0x00 // BE ╛
		,0x00,0x00,0x00,0x00,0xF8,0x18,0x18,0x18 // BF ┐
		,0x18,0x18,0x18,0x18,0x1F,0x00,0x00,0x00 // C0 └
		,0x18,0x18,0x18,0x18,0xFF,0x00,0x00,0x00 // C1 ┴
		,0x00,0x00,0x00,0x00,0xFF,0x18,0x18,0x18 // C2 ┬
		,0x18,0x18,0x18,0x18,0x1F,0x18,0x18,0x18 // C3 ├
		,0x00,0x00,0x00,0x00,0xFF,0x00,0x00,0x00 // C4 ─
		,0x18,0x18,0x18,0x18,0xFF,0x18,0x18,0x18 // C5 ┼
		,0x18,0x18,0x1F,0x18,0x1F,0x18,0x18,0x18 // C6 ╞
		,0x36,0x36,0x36,0x36,0x37,0x36,0x36,0x36 // C7 ╟
		,0x36,0x36,0x37,0x30,0x3F,0x00,0x00,0x00 // C8 ╚
		,0x00,0x00,0x3F,0x30,0x37,0x36,0x36,0x36 // C9 ╔
		,0x36,0x36,0xF7,0x00,0xFF,0x00,0x00,0x00 // CA ╩
		,0x00,0x00,0xFF,0x00,0xF7,0x36,0x36,0x36 // CB ╦
		,0x36,0x36,0x37,0x30,0x37,0x36,0x36,0x36 // CC ╠
		,0x00,0x00,0xFF,0x00,0xFF,0x00,0x00,0x00 // CD ═
		,0x36,0x36,0xF7,0x00,0xF7,0x36,0x36,0x36 // CE ╬
		,0x18,0x18,0xFF,0x00,0xFF,0x00,0x00,0x00 // CF ╧
		,0x36,0x36,0x36,0x36,0xFF,0x00,0x00,0x00 // D0 ╨
		,0x00,0x00,0xFF,0x00,0xFF,0x18,0x18,0x18 // D1 ╤
		,0x00,0x00,0x00,0x00,0xFF,0x36,0x36,0x36 // D2 ╥
		,0x36,0x36,0x36,0x36,0x3F,0x00,0x00,0x00 // D3 ╙
		,0x18,0x18,0x1F,0x18,0x1F,0x00,0x00,0x00 // D4 ╘
		,0x00,0x00,0x1F,0x18,0x1F,0x18,0x18,0x18 // D5 ╒
		,0x00,0x00,0x00,0x00,0x3F,0x36,0x36,0x36 // D6 ╓
		,0x36,0x36,0x36,0x36,0xFF,0x36,0x36,0x36 // D7 ╫
		,0x18,0x18,0xFF,0x18,0xFF,0x18,0x18,0x18 // D8 ╪
		,0x18,0x18,0x18,0x18,0xF8,0x00,0x00,0x00 // D9 ┘
		,0x00,0x00,0x00,0x00,0x1F,0x18,0x18,0x18 // DA ┌
		,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF // DB █
		,0x00,0x00,0x00,0x00,0xFF,0xFF,0xFF,0xFF // DC ▄
		,0xF0,0xF0,0xF0,0xF0,0xF0,0xF0,0xF0,0xF0 // DD ▌
		,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F // DE ▐
		,0xFF,0xFF,0xFF,0xFF,0x00,0x00,0x00,0x00 // DF ▀
		,0x00,0x00,0x76,0xDC,0xC8,0xDC,0x76,0x00 // E0 α
		,0x00,0x78,0xCC,0xF8,0xCC,0xF8,0xC0,0xC0 // E1 ß
		,0x00,0xFC,0xCC,0xC0,0xC0,0xC0,0xC0,0x00 // E2 Γ
		,0x00,0xFE,0x6C,0x6C,0x6C,0x6C,0x6C,0x00 // E3 π
		,0xFC,0xCC,0x60,0x30,0x60,0xCC,0xFC,0x00 // E4 Σ
		,0x00,0x00,0x7E,0xD8,0xD8,0xD8,0x70,0x00 // E5 σ
		,0x00,0x66,0x66,0x66,0x66,0x7C,0x60,0xC0 // E6 µ
		,0x00,0x76,0xDC,0x18,0x18,0x18,0x18,0x00 // E7 τ
		,0xFC,0x30,0x78,0xCC,0xCC,0x78,0x30,0xFC // E8 Φ
		,0x38,0x6C,0xC6,0xFE,0xC6,0x6C,0x38,0x00 // E9 Θ
		,0x38,0x6C,0xC6,0xC6,0x6C,0x6C,0xEE,0x00 // EA Ω
		,0x1C,0x30,0x18,0x7C,0xCC,0xCC,0x78,0x00 // EB δ
		,0x00,0x00,0x7E,0xDB,0xDB,0x7E,0x00,0x00 // EC ∞
		,0x06,0x0C,0x7E,0xDB,0xDB,0x7E,0x60,0xC0 // ED φ
		,0x38,0x60,0xC0,0xF8,0xC0,0x60,0x38,0x00 // EE ε
		,0x78,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0x00 // EF ∩
		,0x00,0xFC,0x00,0xFC,0x00,0xFC,0x00,0x00 // F0 ≡
		,0x30,0x30,0xFC,0x30,0x30,0x00,0xFC,0x00 // F1 ±
		,0x60,0x30,0x18,0x30,0x60,0x00,0xFC,0x00 // F2 ≥
		,0x18,0x30,0x60,0x30,0x18,0x00,0xFC,0x00 // F3 ≤
		,0x0E,0x1B,0x1B,0x18,0x18,0x18,0x18,0x18 // F4 ⌠
		,0x18,0x18,0x18,0x18,0x18,0xD8,0xD8,0x70 // F5 ⌡
		,0x30,0x30,0x00,0xFC,0x00,0x30,0x30,0x00 // F6 ÷
		,0x00,0x76,0xDC,0x00,0x76,0xDC,0x00,0x00 // F7 ≈
		,0x38,0x6C,0x6C,0x38,0x00,0x00,0x00,0x00 // F8 °
		,0x00,0x00,0x00,0x18,0x18,0x00,0x00,0x00 // F9 ∙
		,0x00,0x00,0x00,0x00,0x18,0x00,0x00,0x00 // FA ·
		,0x0F,0x0C,0x0C,0x0C,0xEC,0x6C,0x3C,0x1C // FB √
		,0x78,0x6C,0x6C,0x6C,0x6C,0x00,0x00,0x00 // FC ⁿ
		,0x70,0x18,0x30,0x60,0x78,0x00,0x00,0x00 // FD ²
		,0x00,0x00,0x3C,0x3C,0x3C,0x3C,0x00,0x00 // FE ■
		,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00 // FF  
	};
	uint32_t gUnpackedFont8x8RGBA[ CGA_ATLAS_Z * CGA_TILE_Z ]; // linear 1D 256 glyphs 1x8, 32-bpp

	// Tile Histogram
	uint32_t gHistogram[65536]; // NOTE: Only tiles in texture atlast 0x00{00-1F} - 0x1F{00-1F} are counted.

// Prototypes _________________________________________________________

	void draw_text_centered(const char *pText, const int iRoomX, const int iRoomY);
	void draw_tile(int16_t tile, const int dst_tile_x, const int dst_tile_y);
	void read_map();
	void read_tiles8bpp();

// Utils ______________________________________________________________

	// ========================================
	void convert_tiles_8bpp_32rgba ()
	{
		uint8_t * src = gTilesRawIndex;
		uint32_t* dst = gTilesRGBA;

		for (int y = 0; y < ATLAS_IMAGE_H; ++y)
			for (int x = 0; x < ATLAS_IMAGE_W; ++x)
				*dst++ = gPalette[*src++ & 0xF];
	}

	// ========================================
	int16_t *get_map_start ()
	{
		return (int16_t*)(gRawMap); // 2 byte header is starting player room
	}

	// ========================================
	int get_map_int (int16_t *data)
	{
		int value = *((int32_t *) data);
		return value;
	}

	// ========================================
	RoomDesc_t* get_room_description (const int iRoomX, const int iRoomY)
	{
		RoomDesc_t *pRoom = &gRoomDescriptions[1];

		for (int iRoom = 1; iRoom < NUM_ROOM_DESCRIPTIONS; ++iRoom, ++pRoom )
		{
			if ((pRoom->nRoomX == iRoomX) && (pRoom->nRoomY == iRoomY))
				return pRoom;
		}

		return &gRoomDescriptions[0];
	}

	// ========================================
	size_t read_file (const char* pFilename, void* pBuffer, size_t nBufferSize)
	{
		memset( pBuffer, 0, nBufferSize );

		if (!pFilename)
			return 0;

		FILE *in = fopen( pFilename, "rb" );
		if (in)
		{
			size_t bytes = fread( pBuffer, 1, nBufferSize, in );
			fclose( in );

			return bytes;
		}
		else
			 printf( "ERROR: Couldn't find: '%s'\n", pFilename );

		return 0;
	}

	// ========================================
	void write_file (const char* pFilename, void* pBuffer, size_t nBufferSize)
	{
		FILE *out = fopen( pFilename, "w+b");
		if (out)
		{
			size_t wrote = fwrite( pBuffer, 1, nBufferSize, out );
			fclose( out );
			if (!wrote)
				printf( "ERROR: Wrote zero bytes!\n" );
			if (wrote == nBufferSize)
				printf( "Saved: %s\n", pFilename );
		}
		else
			printf( "ERROR: Couldn't write: '%s'\n", pFilename );
	}

// Font _______________________________________________________________

	// Generate gPackedFont8x8RGBA 
	// ========================================
	void pack_CGA_font ()
	{
		// The game uses these two font files, CGA style, both which are incomplete:
		//
		//   PC_Senior_6.dds
		//   PC_Senior_24.dds
		//
		// Saved as raw, 32-bpp, RGBA
		if ( !read_file( "font_cga_256x64.data", gRawCGAFont, sizeof(gRawCGAFont)) )
			return;

		uint32_t *pSrc = gRawCGAFont;
		uint8_t  *pDst = gPackedFont8x8RGBA;

		// Linearize: 2D interleaved 8-bpp into linear 1D 1-bpp font
		for (int iGlyph = 0; iGlyph < 256; ++iGlyph)
		{
			int iTileY = iGlyph / CGA_ATLAS_W;
			int iTileX = iGlyph % CGA_ATLAS_W;

			///Src = gRawCGAFont + (iTileY * CGA_ROW_H) + (iTileX * CGA_TILE_W);
			pSrc = gRawCGAFont + GET_IMAGE_OFFSET( iTileX, iTileY, CGA_TILE_W, CGA_ROW_H );
	printf( "\t\t" );
			for (int y = 0; y < CGA_TILE_H; ++y)
			{
				uint8_t bits = 0;
				uint8_t mask = 0x80;
				for (int x = 0; x < CGA_TILE_W; ++x, mask >>= 1)
				{
					if (*pSrc & 0xFF) // image is monochrome, only need to look at red channel
						bits |= mask;
					pSrc++;
				}
				*pDst++ = bits;
	printf( ",0x%02X", bits);
				pSrc -= CGA_TILE_W; // start of glyph
				pSrc += CGA_ROW_W ; // next scan line in glyph
			}
	printf( " // %02X %c U+????\n", iGlyph, (iGlyph < 0x1) ? ' ' : iGlyph );
		}
	}

	// Unpack linear 1-bpp font into linear 32-bpp font
	// ========================================
	void unpack_CGA_font ()
	{
		uint8_t  *pSrc = gPackedFont8x8RGBA  ; // linear
		uint32_t *pDst = gUnpackedFont8x8RGBA; // linear

		for (int iGlyph = 0; iGlyph < CGA_ATLAS_Z; ++iGlyph)
		{
			for (int y = 0; y < CGA_TILE_H; ++y)
			{
				uint8_t bits = *pSrc++;
				uint8_t mask = 0x80;
				for (int x = 0; x < CGA_TILE_W; ++x, mask >>= 1)
				{
					if (bits & mask)
						*pDst++ = gPalette[ 15 ]; // white
					else
						*pDst++ = gPalette[  0 ]; // black
				}
			}
		}
#ifdef SAVE_CGA_FONT
	char sFileName[256];
	sprintf( sFileName, "font_cga_rgba32_%dx%d.data", CGA_TILE_W, CGA_ROW_H );
	write_file( sFileName, gUnpackedFont8x8RGBA, sizeof(gUnpackedFont8x8RGBA) );
#endif
	}

// Main _______________________________________________________________

// copy from 1D World Map to 2D World Map
// ========================================
void copy_room (const int iSrcRoom, const int nDstRoomX, const int nDstRoomY )
{
    uint32_t *pSrc = gWorldMap1D + (iSrcRoom  * ROOM1C_PIXELS)                            ; // 1D 320x28608
    uint32_t *pDst = gWorldMap2D + (nDstRoomY * ROOM2D_PIXELS) + (ROOM1C_W_PX * nDstRoomX); // 2D 6080x2600

	for (int y = 0; y < ROOM1C_H_PX; ++y)
	{
		memcpy( pDst, pSrc, ROOM1C_W_PX*4 ); // 4 = RGBA channels
		pSrc += MAP1C_IMAGE_W;
		pDst += MAP2D_IMAGE_W;
	}
}

// ========================================
int count_rooms ()
{
	int16_t *pMap = get_map_start();
	int16_t *pSrc = pMap;
	Room_t  *pRoom  = gRooms;
	int      iRoom = 0;
	int      nRoom = 0;

	memset( gRooms    , 0, sizeof(gRooms    ));
	memset(&gMapHeader, 0, sizeof(gMapHeader));
	memset(&gWorldMeta, 0, sizeof(gWorldMeta));

	gMapHeader = *((MapHeader_t*) pSrc );
	pSrc += (sizeof(MapHeader_t)/2);

	if (gMapHeader.nVersion != META_version)
	{
		printf( "ERROR: Couldn't decode map!\n" );
		return nRoom;
	}
	else
	if (sizeof(gMapHeader) != 22)
	{
		printf( "ERROR: Map header not 22 bytes!\n" );
		return nRoom;
	}
	else
	{
		printf( "Map header:\n" );
		printf( "  Found Player starting room: %d x %d\n", gMapHeader.nPlayerStartRoomX, gMapHeader.nPlayerStartRoomY );
		printf( "  Uknown field 1: %d\n", gMapHeader.nUnknown1 );
		printf( "  Uknown field 2: %d\n", gMapHeader.nUnknown2 );
		printf( "  Total rooms   : %d\n", gMapHeader.nRooms    );
		nRoom = gMapHeader.nRooms;
	}

	for( iRoom = 0; iRoom < nRoom; ++iRoom )
	{
		pRoom->nRoomId   = iRoom;
		pRoom->nRoomX    = get_map_int( pSrc + 0 );
		pRoom->nRoomY    = get_map_int( pSrc + 2 );
		pRoom->pRoomData = pSrc + 4;
		pRoom->nRoomSize = ROOM1C_Z;
		pRoom->pRoomDesc = get_room_description( pRoom->nRoomX, pRoom->nRoomY );

		// Update world size
		if (pRoom->nRoomX < gWorldMeta.nMinRoomX) gWorldMeta.nMinRoomX = pRoom->nRoomX;
		if (pRoom->nRoomY < gWorldMeta.nMinRoomY) gWorldMeta.nMinRoomY = pRoom->nRoomY;
		if (pRoom->nRoomX > gWorldMeta.nMaxRoomX) gWorldMeta.nMaxRoomX = pRoom->nRoomX;
		if (pRoom->nRoomY > gWorldMeta.nMaxRoomY) gWorldMeta.nMaxRoomY = pRoom->nRoomY;

		printf( "@ %06X Room #%3d (%+3d x %+3d), Size: %d, %s\n",
			(int)(pSrc - pMap)*2, pRoom->nRoomId, pRoom->nRoomX, pRoom->nRoomY, pRoom->nRoomSize, pRoom->pRoomDesc->pDesc );
		pSrc += 4 + ROOM1C_Z;
		pRoom++;
	}

	// NOTE: Unknown trailing map data
	int offset = (int)(pSrc - pMap)*2;
	int slack =  (int)(gSize - offset);
	printf( "@ %06X Unknown map data: %d (0x%04X) bytes\n", offset, slack, slack );

	return iRoom;
};

// ========================================
void draw_1D_room (const int iRoom, const int nNextRoomY )
{
	int16_t  iTile;
	Room_t  *pRoom = &gRooms[ iRoom ];
	int16_t *pSrc = pRoom->pRoomData;
#ifdef TUTORIAL // temp disable to dump raw rooms
  #if TUTORIAL < 3
    pSrc = (int16_t*)(gRawMap + iRoom*ROOM1C_Z);
  #else // assume 30 byte map header, room has no header
    pSrc = (int16_t*)(gRawMap + sizeof(MapHeader_t) + 8 + iRoom*ROOM1C_Z);
    // 22 byte header, each room has an 8 byte header
  #endif
#else
	if (pRoom->nRoomSize != ROOM1C_Z)
	{
		printf( "ERROR: Room size != %d tiles\n", ROOM1C_Z );
		return;
	}
#endif

#ifdef TUTORIAL
  #if TUTORIAL == 1
	for (int y = 0; y < ROOM1C_H; y++)
	{
		for (int x = 0; x < ROOM1C_W; x++)
		{
  #else
	for (int x = 0; x < ROOM1C_W; x++)
	{
		for (int y = 0; y < ROOM1C_H; y++)
		{
  #endif
#else
	for (int x = 0; x < ROOM1C_W; x++)
	{
		for (int y = 0; y < ROOM1C_H; y++)
		{
#endif
			// Map contains Tiles that are in Little Endian format: xx yy
			iTile = *pSrc++;
			gHistogram[iTile]++;
			draw_tile(iTile, x, y + nNextRoomY/TILE_H); // Draws room to 1D WorldMap
		}
	}
}

// ========================================
void draw_2d_glyph (uint8_t c, const int iTextX, const int iTextY)
{
	uint32_t *pSrc = gUnpackedFont8x8RGBA + c*CGA_TILE_Z;
	uint32_t *pDst = gWorldMap2D          + iTextY*MAP2D_IMAGE_W + iTextX;

	for (int y = 0; y < CGA_TILE_H; ++y)
	{
		for (int x = 0; x < CGA_TILE_W; ++x)
		{
			*pDst++ = *pSrc++;
		}
		pDst -= CGA_TILE_W;
		pDst += MAP2D_IMAGE_W;
	}
}

// ========================================
void draw_rooms (int nRooms)
{
	memset(gWorldMap1D, 0, sizeof(gWorldMap1D));
	memset(gWorldMap2D, 0, sizeof(gWorldMap2D));
	memset(gHistogram , 0, sizeof(gHistogram) );

	gWorldMeta.nMapWidth  = (gWorldMeta.nMaxRoomX -gWorldMeta.nMinRoomX) + 1;
	gWorldMeta.nMapHeight = (gWorldMeta.nMaxRoomY -gWorldMeta.nMinRoomY) + 1;

	printf( "World size: %d x %d rooms\n", gWorldMeta.nMapWidth, gWorldMeta.nMapHeight );
	printf( "   Left : %+3d, Top: %+3d\n", gWorldMeta.nMinRoomX, gWorldMeta.nMinRoomY );
	printf( "   Right: %+3d, Bot: %+3d\n", gWorldMeta.nMaxRoomX, gWorldMeta.nMaxRoomY );

	int nNextRoomStartY = 0;
	for (int iRoom = 0; iRoom < nRooms; ++iRoom)
	{
		printf( "Drawing room #%d\n", iRoom );
		draw_1D_room( iRoom, nNextRoomStartY );
		nNextRoomStartY += ROOM1C_H_PX;

		RoomDesc_t *pDesc  = gRooms[ iRoom ].pRoomDesc;
		int         nRoomX = pDesc->nRoomX - gWorldMeta.nMinRoomX; // remap [-10,-5] -> [0,0]
		int         nRoomY = pDesc->nRoomY - gWorldMeta.nMinRoomY;

		copy_room( iRoom, nRoomX, nRoomY );                 // Copies room from 1D WorldMap to 2D WorldMap
		draw_text_centered( pDesc->pDesc, nRoomX, nRoomY ); // Draw Text on 2D World Map
	}
}

// Copy from 1D 1x8 @ 32bpp to World Map 2D 6080x2600
// ========================================
void draw_text_centered (const char *pText, const int iRoomX, const int iRoomY)
{
	int nLen   = (int) strlen( pText );
	int iLeftX = (ROOM1C_W - nLen)/2;

	int iCol   = 0;
	int iTextX =  (iRoomX    * ROOM1C_W_PX)             ; // NOTE: Intentional inconsistency
	int iTextY = ((iRoomY+1) * ROOM2D_H_PX) - CGA_TILE_H; // with Room1C_W and Room2D_H

	// blanks on left
	for (iCol = 0; iCol < iLeftX; ++iCol)
	{
		draw_2d_glyph( ' ', iTextX, iTextY );
		iTextX += CGA_TILE_W;
	}

	for (int iGlyph = 0; iGlyph < nLen; ++iGlyph)
	{
		uint8_t c = pText[iGlyph];
		draw_2d_glyph( c, iTextX, iTextY );
		iTextX += CGA_TILE_W;
		iCol++;
	}

	// blanks on right
	for ( ; iCol < ROOM1C_W; ++iCol)
	{
		draw_2d_glyph( ' ', iTextX, iTextY );
		iTextX += CGA_TILE_W;
	}
}


// Copy from 2D 8x8 @ 32bbp gTilesRGBA[] to World Map 1D 320x9999 @ 32bppp gWorldMap1D
// ========================================
void draw_tile (const int16_t iTile, const int iDstTileX, const int iDstTileY)
{
	int iSrcTileX = (iTile >> 0) & 0xFF;
	int iSrcTileY = (iTile >> 8) & 0xFF;

#ifndef TUTORIAL
	// Never should have an invalid atlas tile
	if ((iSrcTileX >= ATLAS_W) || (iSrcTileY >= ATLAS_H))
	{
		printf( "ERROR: Invalid map tile! 0x%04X, DstTileX: %d, DstTileY: %d\n", iTile, iDstTileX, iDstTileY );
		return;
	}
#endif

	iSrcTileX &= (ATLAS_W - 1);
	iSrcTileY &= (ATLAS_H - 1);

	uint32_t *src = gTilesRGBA  + (iSrcTileY * TILE_H * ATLAS_IMAGE_W) + (iSrcTileX * TILE_W);
	uint32_t *dst = gWorldMap1D + (iDstTileY * TILE_H * MAP1C_IMAGE_W) + (iDstTileX * TILE_W);

	for( int y = 0; y < TILE_H; y++ )
	{
		for (int x = 0; x < TILE_W; x++)
		{
			*dst++ = *src++;
		}
		src -= TILE_W; src += ATLAS_IMAGE_W;
		dst -= TILE_W; dst += MAP1C_IMAGE_W;
	}
}

// ========================================
void dump_histogram ()
{
	printf( "Histogram Map Tiles (Tile = 0xYYXX)\n" );

	int used_tiles = 0; // yyxx
	for (int i = 0; i < 0x2000; ++i)
	{
		if (((i >> 8) & 0xFF) >= ATLAS_H) continue;
		if (((i >> 0) & 0xFF) >= ATLAS_W) continue;

		printf( "%04X: %6d  ", i, gHistogram[i] );

		if ((i & 0xF) == 0xF)
			printf( "\n" );

		if( gHistogram[i])
			used_tiles++;
	}

	printf( "Used   tiles: %d\n", used_tiles );
	printf( "Unused tiles: %d\n", NUM_TILE - used_tiles );
	printf( "Total  tiles: %d\n", NUM_TILE );
	printf( "Efficiency: %5.2f%%\n", 100.0 * (double)used_tiles / (double)NUM_TILE );
}

// ========================================
void read_files ()
{
	read_map();
	read_tiles8bpp();
}

// Read the raw binary map
// ========================================
void read_map ()
{
	gSize = read_file( "yhtwtg.map", gRawMap, sizeof( gRawMap ) );
}

// Read texture atlas
// ========================================
void read_tiles8bpp ()
{
	// Note: Source file is `tiles.bmp` but we read a raw image exported with GIMP so we don't need .BMP reading code
	nTilesRawIndex = read_file( "tiles_raw_indexed.data", gTilesRawIndex, 192*K );
}

// Writes the one column Map to a raw 1:1 image file
// ========================================
void write_map1C_rgba32 ()
{
	printf( "Native 1:1 World Map 1D (raw 32-bit RGBA)\n" );
	printf( "  Image Dimensions: %d x %d px\n", MAP1C_IMAGE_W, MAP1C_IMAGE_H );
	printf( "  Rooms: %d x %d\n", MAP1C_ROOM_W, MAP1C_ROOM_H );
	printf( "\n" );

	char sFileName[256];
	sprintf( sFileName, "WorldMap1D_%dx%d_rooms_rgba32_%dx%d.data", MAP1C_ROOM_W, MAP1C_ROOM_H, MAP1C_IMAGE_W, MAP1C_IMAGE_H );
	write_file( sFileName, gWorldMap1D, MAP1C_SIZE );
}

// ========================================
void write_map2D_rgba32 ()
{
	printf( "Native 1:1 World Map 2D (raw 32-bit RGBA)\n" );
	printf( "  Image Dimensions: %d x %d px\n", MAP2D_IMAGE_W, MAP2D_IMAGE_H );
	printf( "  Rooms: %d x %d\n", MAP2D_ROOW_W, MAP2D_ROOM_H );
	printf( "\n" );

	char sFileName[256];
	sprintf( sFileName, "WorldMap2D_%dx%d_rooms_rgba32_%dx%d.data", MAP2D_ROOW_W, MAP2D_ROOM_H, MAP2D_IMAGE_W, MAP2D_IMAGE_H );
	write_file( sFileName, gWorldMap2D, MAP2D_SIZE );
}

void write_map2D_bitmap()
{
	const int BMP_HEADER_SIZE = 54;

	uint32_t aHeader[13]; // Windows .BMP header, "BM" + 12*int32 = 54 bytes
	int     nPlanes   = 1;
	int     nBitcount = 32 << 16; // nBitCount and nPlanes are 16-bit in the header but we pack them together

	// Header: Note that the "BM" identifier in bytes 0 and 1 is NOT included in this header but IS written to the file
	aHeader[ 0] = BMP_HEADER_SIZE + MAP2D_SIZE; // bfSize (total file size)
	aHeader[ 1] = 0;                            // bfReserved1 bfReserved2
	aHeader[ 2] = BMP_HEADER_SIZE;              // bfOffbits
	aHeader[ 3] = 40;                           // biSize BITMAPHEADER
	aHeader[ 4] = MAP2D_IMAGE_W;                // biWidth
	aHeader[ 5] = MAP2D_IMAGE_H;                // biHeight
	aHeader[ 6] = nBitcount | nPlanes;          // biPlanes, biBitcount
	aHeader[ 7] = 0;                            // biCompression
	aHeader[ 8] = MAP2D_SIZE;                   // biSizeImage
	aHeader[ 9] = 0;                            // biXPelsPerMeter
	aHeader[10] = 0;                            // biYPelsPerMeter
	aHeader[11] = 0;                            // biClrUsed
	aHeader[12] = 0;                            // biClrImportant

	uint32_t nFileSize = 54 + MAP2D_SIZE;
	uint8_t *pBuffer   = new uint8_t [ nFileSize ];

	pBuffer[0] = 'B';
	pBuffer[1] = 'M';
	memcpy( pBuffer+2              , &aHeader[0], BMP_HEADER_SIZE-2 );

	// Stupid Windows .BMP are upside down, copy scanline by scanline
	// Otherwise we could simply just write the entire map in one go
	//     memcpy( pBuffer+BMP_HEADER_SIZE, gWorldMap2D, MAP2D_SIZE        );

	uint32_t *pSrc = gWorldMap2D + MAP2D_SIZE/4 - MAP2D_IMAGE_W; // start on bottom scanline, iterate to top
	uint8_t  *pDst = pBuffer + BMP_HEADER_SIZE;

	for (int y = 0; y < MAP2D_IMAGE_H; ++y)
	{
		memcpy( pDst, pSrc, MAP2D_IMAGE_W*4 ); // 4 = RGBA channels

		// Stupid Windows .BMP need to swizzle ABGR -> ARGB otherwise we could do a simple scanline copy
		for (int x = 0; x < MAP2D_IMAGE_W; ++x)
		{
			uint8_t red  = pDst[0];
			uint8_t blue = pDst[2];
			               pDst[2] = red;
			               pDst[0] = blue;
			pDst   += 4;
		}
		pSrc -= MAP2D_IMAGE_W;
	}

	char sFileName[256];
	sprintf( sFileName, "WorldMap2D_%dx%d_rooms.bmp", MAP2D_ROOW_W, MAP2D_ROOM_H );
	write_file( sFileName, pBuffer, aHeader[0] );

	delete [] pBuffer;
}


// Write texture atlas 32 bpp, 256x256 px
// ========================================
void write_tiles32bpp ()
{
	char sFileName[256];
	sprintf( sFileName, "tiles_%dx%d_rgba32_%dx%d.data", ATLAS_W, ATLAS_H, ATLAS_IMAGE_W, ATLAS_IMAGE_H );
	write_file( sFileName, gTilesRGBA, sizeof( gTilesRGBA ) );
}

// ========================================
void write_files ()
{
	write_tiles32bpp();
	write_map1C_rgba32();
	write_map2D_rgba32();

	write_map2D_bitmap();
	dump_histogram();
}

// ========================================
int main(int nArcg, char *aArg[])
{
	char directory[FILENAME_MAX];
	char* path = getcwd(directory, sizeof(directory) - 1);
	printf("Current Directory: %s\n", path);

	//pack_CGA_font();
	unpack_CGA_font();

	read_files();

	int nRooms = count_rooms();
	printf( "Found: %d rooms\n", nRooms );
	if (nRooms != MAP1C_ROOM_H)
		printf( "ERROR: Map contains unexpected number of rooms! %d != %d\n", nRooms, MAP1C_ROOM_H );

	convert_tiles_8bpp_32rgba();
	draw_rooms(nRooms);

	write_files();
	return 0;
}
