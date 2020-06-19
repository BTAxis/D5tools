#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <direct.h>
#include <stdint.h>

FILE *src, *dest, *meta;
int elements, toclen, tocpos, ind, mode, shandle;
uint64_t offset, length;
char fname[64], instring[64], outstring[64], fmetaname[69];
unsigned char* fbuf;

_finddata_t fnd;

void usage(char *a);

int main(int argc, char* argv[])
{
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
        if((shandle = _findfirst(instring, &fnd)) == -1)
            printf("No files found!\n");

        src = fopen(fnd.name, "rb");
        fread(fname, 8, 1, src); //header
        if (strcmp(fname, "PS_FS_V1"))
        {
            printf("Error: Invalid file header.\n");
            fclose(src);
            return -1;
        }

        fread(&elements, 4, 1, src); //Amount of elements.
        toclen = elements * 0x40 + 0x10; //0x30 for the name, then 0x8 for the file length, then 0x8 for the file offset. 0x10 for the TOC header.
        
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
        
        fseek(src, 0x10, SEEK_SET); //Because the header is 0x10 bytes.

        for (int i = 0; i < elements; i++)
        {
            unsigned int val;

            fread(fname, 0x30, 1, src); //File name
            printf("Extracting %s\n", fname);
            fread(&length, 0x8, 1, src); //File length
            fread(&offset, 0x8, 1, src); //Offset within archive
            if (length > fnd.size - offset)
            {
                printf("Error! Target file length out of range.\nOffset: 0x%.2X\n", ftell(src) - 0x8);
                return -1;
            }
            tocpos = ftell(src);
            fseek(src, offset, SEEK_SET);

            strcpy(fmetaname, fname);
            strcat(fmetaname, ".meta");
            meta = fopen(fmetaname, "wb");
            
            for (int j = 0; j < 7; j++)
            {
                fread(&val, 0x4, 1, src);
                fprintf(meta, "%d\n", val);
            }
            
            fclose(meta);
            
            length -= 0x1C;
            fbuf = (unsigned char*) malloc(length);
            fread(fbuf, length, 1, src);
            dest = fopen(fname, "wb");
            fwrite(fbuf, length, 1, dest);
            free(fbuf);
            fseek(src, tocpos, SEEK_SET);
            fclose(dest);
        }
        
        printf("Done.\n");
        fclose(src);
        return 0;
    }
    else if (mode == 2) //Packing mode
    {
        char valline[16], *zero;
        int fsize, TOCsize, val;
        
        dest = fopen(outstring, "wb");
        
        if (chdir(instring))
        {
            printf("\"%s\" does not appear to be a valid directory\n", instring);
            fclose(dest);
            if (remove(outstring))
                perror("Could not delete output file");
            return -1;
        }
        
        if((shandle = _findfirst("*.ogg", &fnd)) == -1)
        {
            printf("No files found! Stop.\n");
            if (remove(outstring))
                perror("Could not delete output file");
            return -1;
        }

        elements = 0;
        do 
        {
            elements++;
        } while (!_findnext(shandle, &fnd));
        
        _findclose(shandle);

        //Start writing the header and TOC.
        fprintf(dest, "PS_FS_V1"); //Archive header
        fwrite(&elements, 8, 1, dest); //Number of files in the archive. This completes the header.
        TOCsize = 0x40 * elements;
        zero = (char*) malloc(TOCsize * sizeof(char));
        memset(zero, 0, TOCsize);
        fwrite(zero, TOCsize, 1, dest);
        
        shandle = _findfirst("*.ogg", &fnd); //No security check because it worked before.
        
        elements = 0;
        do
        {
            printf("Adding %s\n", fnd.name);
            strcpy(fmetaname, fnd.name);
            strcat(fmetaname, ".meta");
            
            src = fopen(fnd.name, "rb");
            if ((meta = fopen(fmetaname, "rb")) == NULL)
            {
                printf("Metadata file for %s not found (should be named %s.meta). Stop.\n", fnd.name, fnd.name);
                fflush(stdout);
                fclose(dest);
                fclose(src);
                if (remove(outstring))
                    perror("Could not delete output file");
                return -1;
            }
            
            offset = ftell(dest);
            fseek(dest, 0x10 + elements * 0x40, SEEK_SET); //Jump back to the corresponding TOC entry
            fwrite(fnd.name, 0x30, 1, dest); //File name.
            fsize = fnd.size + 7 * 4; //File length + metadata (7 * 4 bytes).
            fwrite(&fsize, 4, 1, dest);
            fwrite(zero, 4, 1, dest); //Hack. The file length output is really a 64-bit value, but fnd.size is a 32-bit type.
            fwrite(&offset, 8, 1, dest); //File offset.
            fseek(dest, 0, SEEK_END); //TOC entry done. Proceed with data.
            
            //Write the metadata.
            for (int i = 0; i < 7; i++)
            {
                //Read the metadata value from meta file
                memset(valline, 0, 16);
                if ((fgets(valline, 16, meta)) == NULL)
                {
                    printf("Error reading metadata file %s. Stop.\n", fmetaname);
                    fflush(stdout);
                    fclose(dest);
                    fclose(src);
                    if (remove(outstring))
                        perror("Could not delete output file");
                    return -1;
                }
                val = atoi(valline);
                //Write the metadata value
                fwrite(&val, 4, 1, dest);
            }
            
            //Write the file.
            fbuf = (unsigned char*) malloc(fnd.size);
            fread(fbuf, fnd.size, 1, src);
            fwrite(fbuf, fnd.size, 1, dest);
            free(fbuf);
            
            fclose(src);
            fclose(meta);
            elements++;
        } while (!_findnext(shandle, &fnd));
        printf("Done.\n");
        fclose(dest);
        return 0;
    }
}

void usage(char *a)
{
    printf("Disgaea 5 music archive extractor and repacker. By BTAxis.\n");
    printf("Usage: %s [options] [input] [output]\n", a);
    printf("Options:\n\t-x: extract input archive into output directory\n\t-e: create output archive from input directory\n\n");
}
