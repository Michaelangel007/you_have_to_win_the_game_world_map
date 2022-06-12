# "You Have To Win The Game" native 1:1 World Map 

Did you know that all the online maps for _You Have To Win The Game_ are incomplete? That is, they are missing rooms or show rooms in the wrong location! Nothing earth shattering but does that pique your curiosity? :-)

This reverse engineering document will describe how to turn this raw data ...

```
    00000000: 01 02 FD FF FF FF 00 00  00 00 FB FF FF FF FE FF  ................
    00000010: FF FF 95 00 00 00 F6 FF  FF FF 02 00 00 00 0D 05  ................
    00000020: 0F 05 0C 05 0E 05 0D 05  0E 05 0F 05 0C 05 0F 05  ................
    :
    00046720: 0F 11                                             ..
```

... into this picture:

* ![World Map](pics/WorldMap2D.png)

What makes reverse engineering so challenging but also interesting is that we are pattern matching.  Part of the normal process of reverse engineering is finding out that your assumptions of how the data is laid out is incorrect.   We'll work though the steps of how we identity and fix those assumptions.  While reverse engineering can be a lot of trial-and-error we can be successful through persistance and lateral thinking.


1\. Raw Assets

 The first step is getting images.  We could manually save the image of every room but that is a lot of work for something that can be automated.
 
 Fortunately the game includes a built in console that can be activated with `~` (tilde).  The command to dump all the game files is: `dumpallcontent`


 * ![dumpallcontent](pics/dumpallcontent_done.png)

 On Windows this will write all the games files to: `%USERPROFILE%\Documents\My Games\You Have to Win the Game\Content Dump\`
 
 The 3 files we are interested in are:
 
 * tiles.bmp
 * Rooms_Normal.xml
 * yhtwtg.map

2\. Texture Atlas

 The game has all the art for rooms in a single image called a texture atlast.  The tiles.bmp looks like this:

 * ![Tiles sans header](pics/tiles_sans_header.png)

 Let's add column and row headers to the tiles to make it easier to reference them:

 * ![Tiles with header](pics/tiles_with_header.png)

 Examing the tiles we see that the texture atlas is 256x256 px with 32x32 tiles.  That means each tile is 8x8 px.  This will help us figure out a room dimension.

3\. Room dimensions

 Let's examine a screenshot of a room:

 * ![Room 3 sans grid](pics/room3_sans_grid.png)
 
 Manually counting the columns and rows let's us see that a room is 40x25 tiles. Let's add in grid lines for the room tiles:

 * ![Room 3 with grid](pics/room3_with_grid.png)

  Examining the map data we _don't_ see the room names in there which means they are probably in a different file.

4\. Grid of the World

 If we look at `Rooms_Normal.xml` we have World X and Y coordinates for each room along with a description.
 
```
<room x="-3" y="0" title="You Have to Start the Game">
```

If we focus on just the room meta data this is what our file looks like after getting rid of all the extra data, fixing the x and y fields to be padded, and then sorting by x, then y:

```
<room x="-10" y=" 2" title="Slippery Slope" />
<room x="-10" y=" 3" title="Nice of You to Drop In">

<room x=" -9" y=" 2" title="Hollow King">
<room x=" -9" y=" 3" title="Foot of the Throne">
<room x=" -9" y=" 4" title="Welcome to the Underground">
<room x=" -9" y=" 5" title="Long Way Down" />
<room x=" -9" y=" 6" title="Secret Passage" />
 :
<room x="  8" y="-3" title="Melancholy" secret="true">
<room x="  8" y="-2" title="Sadness" secret="true" onpause="sigil_end">

<room x="  8" y=" 0" title="Brazen Machines">
<room x="  8" y=" 1" toptitle="Spider Gloves" title="- Cling to Walls and Leap Off -">
<room x="  8" y=" 2" title="Not Worth It!">

```

 Drawing a grid of all the rooms we find we have this 2D world map:
 
 * ![World Map with names](pics/world_map_room_names.png)

 We won't immediately use this but it will give us a sense of how the rooms should fit together when we go to "stitch" them together to make our native 1:1 world map.

5\. Map Export Take 1

 Instead of generating a 2D grid of rooms it is far easier to make sure we can properly "decode" a single room first. We will store these rooms in a single column format.

 Recall that our texture atlas of tiles is 32x32 tiles.  This means the map format is probably an array of 16-bit values either in:
 
 * XX YY, or
 * YY XX  format.

 Looking at the file size of yhtwtg.map we see it is:
 
 * 288,546 bytes.
 
 A room, without the text description, is 40x24 = 960 tiles where each tile is 2 bytes, for a total of 1920 bytes/room

 Thus our map should have:
 
```
    = 288,546 bytes / 1920 bytes/room
    = 150.284375 rooms
```

Hmm, that means we have extra "slack" or potentially unused data.  Assuming the data isn't compressed we should have either ~149 or ~150 rooms.

We'll pretend we have 149 rooms.

It is easy to enumerate through a room, drawing each tile one by one.

```
void draw_1D_room (const int iRoom, const int nNextRoomY )
{
    int16_t pSrc = (int16_t*)(gRawMap + iRoom*ROOM1C_Z);
    for (int y = 0; y < ROOM1C_H; y++)
    {
        for (int x = 0; x < ROOM1C_W; x++)
        {
            int16_t   iTile = *pSrc++;
            draw_tile(iTile, x, y + nNextRoomY/TILE_H); 
        }
    }
}
```
 
We'll save our image of a single column of rooms in a raw 32-bit format with dimensions: 

```
   40 tiles width * 8 px = 320 px width
   24 tiles height * 8 px = 192 px height /room
   
   192 px/room * 149 px = 28608 px total height.
```

We can import our 320x28608 image into GIMP:

* ![Single Column import](pics/import_1a.png)

GIMP defaults to a 24-bit RGB format but we need a 32-bit RGBA format.

* ![Single Column import](pics/import_1b.png)

The color looks good.  Our image is "out of sync" due to the default width being 350.  Our room has a width of 320 px across ...

* ![Single Column import](pics/import_1c.png)


... and our height is 28608 px.

* ![Single Column import](pics/import_1d.png)

Here is the final image:

* ![Single Column import](pics/import_1e_worldmap.png)

Hmm, zooming into the first few rooms we see something isn't correct:

* ![Single Column import](pics/import_1f_bad.png)

How do we fix this?

6\. Map Export Take 2

 In order to identity a room format we want to search for "unique" or "rare" tiles.  If we look at room 2, "Kiss Principle" at (0,-2), we see there are some signs in the room:
 
 * ![Room 2](pics/room2.png)

 Let's see if we can find out where the tiles for "TOUCH" are located in the map file.

 Referring back to our texture atlas ...

* ![Touch tiles](pics/touch_tiles.png)
 
 ... this means we should have 3 consecutive tiles:
 
 * 0x0210, 0x0211, 0x0212, or
 * 0x1002, 0x1102, 0x1202.
 
Fortunaly this sequence of bytes is rather uncommon otherwise we may get a lot of false positives when searching.

If you don't have a binary editor that has search capability we can hexdump the map and then search the text file.  Searching for `02 10` we don't find anything but we DO find `10 02` at offset 0x1E4DC:

```
0001E4D0: 00 00 00 00 00 00 00 00  00 00 01 0D 10 02 00 00  ................
0001E4E0: 00 00 00 00 00 00 04 08  03 0D 14 01 00 00 00 00  ................
0001E4F0: 01 0A 02 0B 00 00 00 00  00 00 00 00 00 00 00 00  ................
0001E500: 00 00 00 00 00 00 00 00  00 00 00 00 11 02 00 00  ................
0001E510: 00 00 00 00 00 00 03 08  00 00 15 01 00 00 00 00  ................
0001E520: 01 0A 03 0B 00 00 00 00  00 00 00 00 00 00 00 00  ................
0001E530: 00 00 00 00 00 00 00 00  00 00 00 00 12 02 00 00  ................
```

What is strange is that 0x1102 and 0x1202 are not consequitive?!

Counting the gap of tiles (remember each tile is 2 bytes each) between them we have: 3*8 = 24 tiles.

Hmm, our room height is 24 tiles.  Does this mean our room data is in column format?

Let's test our premise.

```
void draw_1D_room (const int iRoom, const int nNextRoomY )
{
    int16_t pSrc = (int16_t*)(gRawMap + iRoom*ROOM1C_Z);
    for (int x = 0; x < ROOM1C_W; x++)
    {
        for (int y = 0; y < ROOM1C_H; y++)
        {
            int16_t   iTile = *pSrc++;
            draw_tile(iTile, x, y + nNextRoomY/TILE_H); 
        }
    }
}
```

Importing our single room image again into GIMP:

* ![Single Column import](pics/import_2a.png)

Switching to 32-bit RGBA:

* ![Single Column import](pics/import_2b.png)

Setting the width:

* ![Single Column import](pics/import_2c.png)

And height:

* ![Single Column import](pics/import_2d.png)

Yes! Progress.

* ![Single Column import](pics/import_2e_worldmap.png)

However looking at the rooms ... 

* ![Single Column import](pics/import_2f_better.png)

.. there are 2 problems:

* The first few columns look like junk
* The left edge of the room gets out of "sync".  It slowly drifts.

7. Decoding a room proper

A common file format is:

```
   +--------+
   | header |
   +--------+
   | data   |
   +--------+
```

If we play around skipping various bytes of the header so that room 1 draws correctly we eventually discover that the map has a header size of 30 bytes.

* ![Colorized hexdump of the map header](pics/colorized_hexdumpdump_header30.png)

Unfortunately that doesn't fix the 2nd room:

* ![Room 2 broken](broken_room2.png)

It looks like each room has meta-data?  Looking at the raw hexdump between the 1st and 2nd room we see this:

```
00000790: 0F 05 0C 05 0D 05 0C 05  0C 05 0E 05 0C 05 F6 FF  ................
000007A0: FF FF 03 00 00 00 0D 05  0D 05 0F 05 0D 05 0C 05  ................
```

Let's colorize this hex dump for readability (room 1 is red, room 2 is green)

* ![Colorized hexdump room1and2](pics/colorized_hexdump_room1and2.png)

We notice something that looks like signed 32-bit integers?

* FFFFFFF6 = -10
* 00000003 = 3

Hmmm, looking back at simplified our `Rooms_Normal.xml` that turned into a world grid ...

```
        , { -10, 2, "Slippery Slope"                    }
        , { -10, 3, "Nice of You to Drop In"            }
```
... it looks like each map has RoomX, and RoomY cooridinate before the raw room tiles!

This mean the map format is (not to scale):


```
   +-------------------------------+
   | map header                    |
   +---------------+---------------+
   | room 1 header | room 1 data   | \
   +---------------+---------------+  \
   | room 2 header | room 2 data   |   \
   +---------------+---------------+     N rooms
   :                               :   /
   +---------------+---------------+  /
   | room N header | room N data   | /
   +---------------+---------------+
```

This also means that our original assumption that the map header being 30 bytes is actually 8 bytes TOO big since those 8 bytes belong to the map 1 header.

* ![Colorized hexdump of the map header](pics/colorized_hexdumpdump_header22.png)

8. Rom descriptions

Now that we have the image of a single column of rooms we can create our master image.  One minor detail is that a room in the single column image is 320x192 whereas in the 2D grid it is 320x200 because we want to include the room description. (24 tiles tall vs 25 tiles tall)

We'll use the same CGA 8x8 font that the game uses.

The game uses the font file `Senior_24.dds` which is an image in a DirectX format. It has two minor problems:

* It doesn't have all 256 glyphs.
* It has a non-standard 15 characters per row instead of the standard 16x16 character grid.

Nerdy Pleasures has a [fantastic article](http://nerdlypleasures.blogspot.com/2015/04/ibm-character-fonts.html)  about the IBM PC fonts.  Did you know that there were TWO CGA fonts?!

* ![Original CGA Font](pics/cga_font_original.png)
* ![Revised CGA Font](pics/cga_font_revised.png)

A total of 5 glyphs were changed between the original and revised CGA font.

* Diamond
* Clubs
* Spades
* White Sun glyph
* Uppercase `S`

```
     Original       Revised

     01234567       01234567  
    +--------+     +--------+ 
   0|   X    |0   0|   X    |0
   1|  XXX   |1   1|  XXX   |1
   2| XXXXX  |2   2| XXXXX  |2
   3|XXXXXXX |3   3|XXXXXXX |3
   4| XXXXX  |4   4| XXXXX  |4
   5|  XXX   |5   5|  XXX   |5
   6|   X    |6   6|   X    |6
   7|    X   |7   7|        |7
    +--------+     +--------+ 
     01234567      01234567   

      01234567       01234567  
     +--------+     +--------+ 
    0|  XXX   |0   0|  XXX   |0
    1| XXXXX  |1   1| XXXXX  |1
    2|  XXX   |2   2|  XXX   |2
    3|XXXXXXX |3   3|XXXXXXX |3
    4|XXXXXXX |4   4|XXXXXXX |4
    5| XXXXX  |5   5| X X X  |5
    6|  XXX   |6   6|   X    |6
    7| XXXXX  |7   7|  XXX   |7
     +--------+     +--------+ 
      01234567      01234567   

      01234567       01234567  
     +--------+     +--------+ 
    0|   X    |0   0|   X    |0
    1|   X    |1   1|   X    |1
    2|  XXX   |2   2|  XXX   |2
    3| XXXXX  |3   3| XXXXX  |3
    4|XXXXXXX |4   4|XXXXXXX |4
    5| XXXXX  |5   5| XXXXX  |5
    6|  XXX   |6   6|   X    |6
    7| XXXXX  |7   7|  XXX   |7
     +--------+     +--------+ 
      01234567      01234567   

      01234567       01234567  
     +--------+     +--------+ 
    0|X  XX  X|0   0|   XX   |0
    1| X XX X |1   1|XX XX XX|1
    2|  XXXX  |2   2|  XXXX  |2
    3|XXX  XXX|3   3|XXX  XXX|3
    4|XXX  XXX|4   4|XXX  XXX|4
    5|  XXXX  |5   5|  XXXX  |5
    6| X XX X |6   6| X XX X |6
    7|X  XX  X|7   7|X  XX  X|7
     +--------+     +--------+ 
      01234567      01234567   

     01234567       01234567  
    +--------+     +--------+ 
   0| XXXX   |0   0| XXXX   |0
   1|XX  XX  |1   1|XX  XX  |1
   2|XXX     |2   2| XX     |2
   3| XXX    |3   3|  XX    |3
   4|   XXX  |4   4|   XX   |4
   5|XX  XX  |5   5|XX  XX  |5
   6| XXXX   |6   6| XXXX   |6
   7|        |7   7|        |7
    +--------+     +--------+ 
     01234567      01234567   
```

Here is an animation showing the original and revised font:

* ![CGA Font Comparison](pics/cga_font_compare.gif)

Our CGA font has the fixed clubs, diamonds, spades, and `S` of the revised font but the white sun glyph of the original:

* ![CGA Font Custom](pics/cga_font_custom.png)

9. Native 1:1 World Map

With this we have a final native 1:1 world map that was shown at the top of the document.
