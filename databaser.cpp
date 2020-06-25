#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <direct.h>

void bufread(void *tbuf, unsigned char *sbuf, long *offset, long length);
void bufwrite(unsigned char *dbuf, void *source, long *offset, long length);
static void ror(unsigned char *p, unsigned int pos, unsigned int size, unsigned char key);
static void exor(unsigned char *p, unsigned int pos, unsigned int size, unsigned char key);
void usage(char *a);

int main(int argc, char* argv[])
{
    FILE *src, *dest;
    int ind = 0, mode, shandle, offset, length, crc, members;
    long tell;
    char fname[32], instring[64], outstring[64];
    unsigned char *fbufin, *fbufout;

    _finddata_t fnd;

    if (argc < 2)
    {
        usage(argv[0]);
        return -1;
    }

    if (argv[1][0] != '-' || strlen(argv[1]) < 2)
    {
        printf("No options specified! I don't know what you want me to do.\n");
        return -1;
    }
    ind++;

    while (ind < strlen(argv[1]))
    {
        switch (argv[1][ind])
        {
            case '?':   usage(argv[0]);
                        return 0;
            case 'x':   mode = 1;
                        break;
            case 'e':   mode = 2;
                        break;
            default:    printf("Unknown option: %c\n", argv[1][ind]);
                        return -1;
        }
        ind++;
    }
    
    if (argc < 4)
    {
        printf("Insufficient parameters.\n");
        return -1;
    }
    
    strcpy(instring, argv[2]);
    strcpy(outstring, argv[3]);
    
    if(mode == 1) //Extraction mode
    {
        long insize, inndex = 0;

        if((shandle = _findfirst(instring, &fnd)) == -1)
            printf("No files found!\n");

        src = fopen(fnd.name, "rb");
        insize = fnd.size;
        fbufin = (unsigned char*) malloc (insize);
        fread(fbufin, insize, 1, src);
        fclose(src);
        
        fbufout = (unsigned char*) malloc(12);
        bufread(fbufout, fbufin, &inndex, 12); //Header
        
        if (strcmp((char*) fbufout, "NISPACK"))
        {
            //Unscramble the input stream.
            exor(fbufin, 0, insize, 0xF0);
            ror(fbufin, 0, insize, 4);
            exor(fbufout, 0, 12, 0xF0);
            ror(fbufout, 0, 12, 4);
        }
        
        if (strcmp((char*) fbufout, "NISPACK"))
        {
            printf("Error: Invalid file header.\n");
            return -1;
        }
        
        if (chdir(outstring))
        {
            if (mkdir(outstring))
            {
                printf("Unable to create directory! Aborting.\n");
                fclose(src);
                return -1;
            }
            chdir(outstring);
        }
        
        bufread(&members, fbufin, &inndex, 4); //Members to process.
        
        for (int i = 0; i < members; i++)
        {
            bufread(fname, fbufin, &inndex, 0x20);
            bufread(&offset, fbufin, &inndex, 4);
            bufread(&length, fbufin, &inndex, 4);
            bufread(&crc, fbufin, &inndex, 4);
            
            printf("Extracting %s\n", fname);

            tell = inndex;
            inndex = offset;
            
            fbufout = (unsigned char*) realloc (fbufout, length);
            bufread(fbufout, fbufin, &inndex, length); //File data
            
            dest = fopen(fname, "wb");
            fwrite(fbufout, length, 1, dest);
            fclose(dest);
            
            inndex = tell;
        }
        
        printf("Done.\n");
        return 0;
    }
    else if (mode == 2) //Packing mode
    {
        long outsize = 0, outdex = 0, member = 0;
        
        dest = fopen(outstring, "wb");
        
        if (chdir(instring))
        {
            printf("\"%s\" does not appear to be a valid directory\n", instring);
            fclose(dest);
            if (remove(outstring))
                perror("Could not delete output file");
            return -1;
        }
        
        if((shandle = _findfirst("*", &fnd)) == -1)
        {
            printf("No files found! Stop.\n");
            if (remove(outstring))
                perror("Could not delete output file");
            return -1;
        }

        members = 0;
        do 
        {
            if (strcmp(fnd.name, ".") && strcmp(fnd.name, ".."))
            {
                outsize += fnd.size;
                members++;
            }
        } while (!_findnext(shandle, &fnd));
        
        _findclose(shandle);
        
        fbufout = (unsigned char*) malloc (outsize + 0x10 + 0x2C * members); //Allocate for the TOC and all the files.
        fbufin = (unsigned char*) malloc (12);
        memset(fbufout, 0, outsize);
        memset(fbufin, 0, 12);
        strcpy((char*) fbufin, "NISPACK");
        
        bufwrite(fbufout, fbufin, &outdex, 12); //Add the header and the zeroes preceding the member count.
        bufwrite(fbufout, &members, &outdex, 4);
        
        shandle = _findfirst("*", &fnd); //No security check because it worked before.
        
        offset = 0x10 + 0x2C * members; //The first file offset is right after the last TOC member.
        crc = 0x4D757E74; //This is the value used in the original, but the game will accept anything, apparently.
        
        do 
        {
            if (strcmp(fnd.name, ".") && strcmp(fnd.name, ".."))
            {
                printf("Adding %s\n", fnd.name);
                
                bufwrite(fbufout, fnd.name, &outdex, 0x20);
                bufwrite(fbufout, &offset, &outdex, 0x4);
                bufwrite(fbufout, &fnd.size, &outdex, 0x4);
                bufwrite(fbufout, &crc, &outdex, 0x4);
                
                tell = outdex;
                outdex = offset;
                
                src = fopen(fnd.name, "rb");
                fbufin = (unsigned char*) realloc(fbufin, fnd.size);
                fread(fbufin, fnd.size, 1, src);
                bufwrite(fbufout, fbufin, &outdex, fnd.size);
                
                offset = outdex;
                outdex = tell;
                
            }
        } while (!_findnext(shandle, &fnd));
        
        //Scramble the output buffer.
        ror(fbufout, 0, outsize, 4); //Should really use rotate left, but eh, result is the same anyway since we're just swapping half-bytes around.
        exor(fbufout, 0, outsize, 0xF0);
        fwrite(fbufout, outsize, 1, dest);

        printf("Done.\n");
        fclose(dest);
        return 0;
    }
}

/*
 * Read an arbitrary number of bytes into an arbitrary data type.
*/
void bufread(void *destination, unsigned char *sbuf, long *offset, long length)
{
    if (*offset + length > _msize(sbuf))
    {
        printf("Error: trying to read beyond end of source buffer. Stop.\n");
        exit(-1);
    }
    memcpy(destination, sbuf + *offset, length);
    *offset += length;
}

/*
 * Add an arbitrary amount of data to a buffer.
*/
void bufwrite(unsigned char *dbuf, void *source, long *offset, long length)
{
    if (*offset + length > _msize(dbuf))
    {
        printf("Error: trying to write beyond end of destination buffer. Stop.\n");
        exit(-1);
    }
    memcpy(dbuf + *offset, source, length);
    *offset += length;
}

/*
 * Bitwise rotate right.
*/
static void ror(unsigned char *p, unsigned int pos, unsigned int size, unsigned char distance)
{
	p += pos;
	while(pos < size)
    {
		*p = (*p >> distance) | (*p << (8 - distance));
        p++;
		pos++;
	}
}

/*
 * Bitwise exclusive OR
*/
static void exor(unsigned char *p, unsigned int pos, unsigned int size, unsigned char key)
{
	p += pos;
	while(pos < size)
    {
		*p ^= key;
		p++;
		pos++;
	}
}

void usage(char *a)
{
    printf("Disgaea 5 database extractor and packer. By BTAxis.\n");
    printf("Usage: %s [options] [input] [output]\n", a);
    printf("Options:\n\t-x: extract input archive into output directory\n\t-e: create output archive from input directory\n\n");
}
