## A small assortment of utilities for modding Disgaea 5 ##
These are a few simple extraction and repacking tools for certain archives used by the PC version of Disgaea 5 Complete.

All C++ based tools have the same usage syntax:
```
tool.exe [options] [input] [output]
Options:
        -x: extract input archive into output directory
        -e: create output archive from input directory
```

So, when extracting an archive, you would call the program with -x, followed by the path to an appropriate archive file, followed by a path to a directory to extract into. When repacking, you would use -e and pass the aforementioned paths in the opposite order.

The tools aren't very smart and expect to find only data they can use in the specified directory when packing, and the data must be formatted exactly right. If badly formatted or unrelated data is in there, or if data is missing that should be there, bad things will probably happen.

Below is some limited documentation on each tool.

### databaser ###
This tool de-scrambles and extracts NISPACK type data files. It can also re-build the files from the member files. The database files themselves are not touched and must be handled with other tools, or by hand. Compatible files include:
- Database_0.dat
- ANM_HI_\*.dat
- Object_\*.dat
- Start_\*.dat
- charworld.dat, charworld_opt.dat, Effect.dat, Texture.dat and Texture_opt.dat, which are contained in Start_JP.dat

### musicDBer ###
This tool picks apart music.dat (which is a member database of Database_0.dat), and puts it back together. It generates one file for each music track exposed to the player via the music gallery (which is all but a small number of tracks). The extension for the output files is .music, but they're just plain text files.

Output looks like this:

```
file=102
auto-unlock=false
title_JP=ＢＧＭ２９
title_EN=BGM 29
title_FR=BGM 29
title_CHN=ＢＧＭ２９
title_KOR=BGM 29
description_JP=小さなダイアリー
description_EN=Small Diary
description_FR=Petit Agenda
description_CHN=小小的日記
description_KOR=작은 다이어리
ID=28
mystery=100
```
- **file** refers to the file name as used in Sound_BGM_0.dat. More on that below. Of note here are the RANDOM and RANDOM MEDLEY "tracks", which are special. These have **file** values 400 and 401. These are likely magic numbers, as I can't see any other way for the game to distinguish them from normal music.
- **auto-unlock** is a flag that determines if the track is available to the player from the start. If false, the track will be unlocked by progressing through the game. When adding new tracks, always set this to true.
- The five **title** lines are the track title as found in the music gallery, one for each language. The encoding for this text is UTF-8.
- The five **description** lines are the track descriptions as found in the music gallery, one for each language. The encoding for this text is UTF-8.
- **ID** is the track's internal identifier. The game will accept multiple tracks with the same ID, but they will be turned on/off for random playback as one. Possibly that also means that only one of the tracks will ever be selected for random play. Suffice it to say tracks should all have unique IDs, except for RANDOM and RANDOM MEDLEY, which share the same ID. Tracks with an ID greater than 151 do not seem to be picked up by the game.
- **mystery** is a number I haven't been able to determine the purpose of. Among the stock tracks, it is 100, 110, 120, 130, 140 or 150. It seems to have no effect on playback at all.

There is one more metadata value that is not included in the file, which is **list order** for the music gallery menu. I have chosen not to expose this value, but rather take the list order from the order of the output files. This does mean that a few tracks will be switched around in the music gallery, but playback is not affected. So, if you want to reorder tracks in the list, change the file names so they sort differently (alphabetically).

### musicer ###
This tool extracts and repacks Sound_BGM_0.dat and Sound_BGM_1.dat. These files store the .ogg music files used by the game.

Output is the .ogg file, along with a metadata file. The .ogg file name will be a number, which is the same number referenced by the music.dat data above. For example, if a **file** entry in the music database is file=102, that music entry will play 102.ogg. As mentioned above, it's best to avoid 400 and 401.

The metadata for music tracks looks like this:
```
7
3191947
113536
8373287
640091
1620526
931330 
```
In order, these values mean:
- Value 1: Always 7.
- Value 2: File size in bytes. Equal to values 5+6+7.
- Value 3: Always 113536.
- Value 4: Total song length in samples. The game does not seem to care if this is accurate and will happily accept 0.
- Value 5: File size in bytes for subtrack 1. If there is no such track, this value is 0.
- Value 6: File size in bytes for subtrack 2. This value can break the game if mismanaged, so get it right.
- Value 7: File size in bytes for subtrack 3. If there is no such track, this value is 0.

A music file consists of up to 3 subtracks. The first subtrack is the intro, the second subtrack is the song main body, and the third subtrack is the outro. The first subtrack is played once, if present, and leads directly into the second subtrack. The subtrack loops back on itself. The third subtrack is thus never actually played, and can be ignored.

To create a file like that from its constituent tracks, simply concatenate the .ogg files together.

### vagger ###
Called that way because sound effect files for Disgaea 5 have a .vag extension, and also because maturity, much like sanity, is for the weak. It is used on Sound_SE_0.dat.

It basically works like musicer, except it automatically renames the sound effects to have a .wav extension, and back. There is also metadata output for the sound files, which looks like this:
```
5
16456
48000
29765
0
16456
0 
```
In order, these values mean:
- Value 1: Always 5.
- Value 2: File size in bytes.
- Value 3: The sample rate of the sound file.
- Value 4: Presumably the length of the sound in samples, though it seems off by a tiny amount.
- Value 5: Always 0.
- Value 6: File size in bytes, identical to value 2.
- Value 7: Always 0. 

### dat2csv ###
This is a Perl script that converts database files (which are retrieved from Database_0.dat) into .csv spread sheets, and back.

Its usage syntax is:
```
dat2csv.pl <-decode/-encode> [options]
      Options:
        -decode: decode D5 database files into human-readable csv files.
        -encode: encode csv files into D5 database files).
        -scheme=<path>: Scheme file to use. If unspecified, './database_0.scheme' will be used.
        -basedir=<path>: path to use for D5 database files. If unspecified, current directory will be used.
        -csvdir=<path>: path to use for csv files. If unspecified, current directory will be used.
        -verbose: print verbose debug output.
        -help: print this message.
```

All options are optional, except for -encode and -decode, of which you must specify exactly one. You can abbreviate options, so '-d' is equivalent to '-decode' and '-s mypath' is equivalent to '-scheme=mypath'.

The program needs to be provided with a scheme file, which defines the layout of the database files to convert. Multiple definitions may exist in sequence within the same file. A scheme definition looks like this:
```
[db=music.dat csv=music.csv]
unused,					int	
unused,					int	
file,					int	
Unlock,					int	
trackname_JP,			string,		44
trackname_EN,			string,		44
trackname_FR,			string,		44
trackname_CHN,			string,		44
trackname_KOR,			string,		44
trackdesc_JP,			string,		0xA0
trackdesc_EN,			string,		0xA0
trackdesc_FR,			string,		0xA0
trackdesc_CHN,			string,		0xA0
trackdesc_KOR,			string,		0xA0
ID,				    	short
???,					short
List #,					int
unused,					int	
```

The first line tells the program which file names to use for input and output. The lines below that define, in order, the data fields found in each entry of the database file. The fields are the label for the data field, the field's type, and if the type is a string, the size (in bytes) allocated for that string. All values must be separated by commas and an arbitrary amount of whitespace. Labels do not have to be unique; they are just used for the headings in the spread sheet.

Available types are:
- quad: A signed 64-bit integer, 8 bytes.
- uquad: An unsigned 64-bit integer, 8 bytes.
- int: A signed integer, 4 bytes.
- uint: An unsigned integer, 4 bytes. Shouldn't really be needed in most cases.
- short: A signed short, 2 bytes.
- ushort: An unsigned short, 2 bytes.
- char: A signed char, 1 byte.
- uchar: An un signed char, 1 byte.
- string: what it says on the tin. You must also specify a size for the string. Sizes may be specified in decimal or hexadecimal notation.

Not that this example provides the same basic functionality as the musicDBer tool above, but it outputs a .csv file instead of creating a separate file for each entry.

## Building ##
The code is simple C++. It uses some Windows-specific functions, and thus is not portable. I built my binaries with MinGW, but any compiler on Windows should work.
