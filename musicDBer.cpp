#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <direct.h>

void parsefail(char* fn, char* found);
void bufread(void *tbuf, unsigned char *sbuf, long *offset, long length);
void bufwrite(unsigned char *dbuf, void *source, long *offset, long length);
void usage(char *a);

int main(int argc, char* argv[])
{
    FILE *src, *dest;
    int ind = 0, mode, shandle, members;
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
        long inndex;
        unsigned char text[0xA0];

        if((shandle = _findfirst(instring, &fnd)) == -1)
            printf("No files found!\n");
        
        src = fopen(fnd.name, "rb");
        
        fread(&members, 4, 1, src); //Amount of elements.
        
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
        
        fbufin = (unsigned char*) malloc (0x418); //Each element is exactly this long.

        for (int i = 0; i < members; i++)
        {
            char fnum[10];
            int val;
            
            fread(fbufin, 0x418, 1, src);
            inndex = 0;
            sprintf(fnum, "%03d", i);
            strcat(fnum, ".music");
            dest = fopen(fnum, "wb");
            printf("Extracting %s\n", fnum);
            
            
            bufread(&val, fbufin, &inndex, 4); //Always 0.
            bufread(&val, fbufin, &inndex, 4); //Always 0.
            bufread(&val, fbufin, &inndex, 4); //Music file identifier
            fprintf(dest, "file=%d\n", val);
            bufread(&val, fbufin, &inndex, 4); //Flag for auto-unlock
            fprintf(dest, "auto-unlock=%s\n", val==3 ? "true" : "false");

            memset(text, 0, 0xA0);
            bufread(text, fbufin, &inndex, 0x2C); //Track name JP
            fprintf(dest, "title_JP=%s\n", text);
            memset(text, 0, 0xA0);
            bufread(text, fbufin, &inndex, 0x2C); //Track name EN
            fprintf(dest, "title_EN=%s\n", text);
            memset(text, 0, 0xA0);
            bufread(text, fbufin, &inndex, 0x2C); //Track name FR
            fprintf(dest, "title_FR=%s\n", text);
            memset(text, 0, 0xA0);
            bufread(text, fbufin, &inndex, 0x2C); //Track name CHN
            fprintf(dest, "title_CHN=%s\n", text);
            memset(text, 0, 0xA0);
            bufread(text, fbufin, &inndex, 0x2C); //Track name KOR
            fprintf(dest, "title_KOR=%s\n", text);
            
            memset(text, 0, 0xA0);
            bufread(text, fbufin, &inndex, 0xA0); //Track description JP
            fprintf(dest, "description_JP=%s\n", text);
            memset(text, 0, 0xA0);
            bufread(text, fbufin, &inndex, 0xA0); //Track description EN
            fprintf(dest, "description_EN=%s\n", text);
            memset(text, 0, 0xA0);
            bufread(text, fbufin, &inndex, 0xA0); //Track description FR
            fprintf(dest, "description_FR=%s\n", text);
            memset(text, 0, 0xA0);
            bufread(text, fbufin, &inndex, 0xA0); //Track description CHN
            fprintf(dest, "description_CHN=%s\n", text);
            memset(text, 0, 0xA0);
            bufread(text, fbufin, &inndex, 0xA0); //Track description KOR
            fprintf(dest, "description_KOR=%s\n", text);
            
            val = 0;
            bufread(&val, fbufin, &inndex, 2); //Unique ID
            fprintf(dest, "ID=%d\n", val);
            bufread(&val, fbufin, &inndex, 2); //Unknown...
            fprintf(dest, "mystery=%d\n", val);
            bufread(&val, fbufin, &inndex, 2); //List order
            //The remaining 6 bytes are always 0.
            fclose(dest);
        }
        printf("Done.\n");
    }
    else if (mode == 2) //Packing mode
    {
        int val = 0;

        dest = fopen(outstring, "wb");
        
        if (chdir(instring))
        {
            printf("\"%s\" does not appear to be a valid directory\n", instring);
            fclose(dest);
            if (remove(outstring))
                perror("Could not delete output file");
            return -1;
        }
        
        if((shandle = _findfirst("*.music", &fnd)) == -1)
        {
            printf("No files found! Stop.\n");
            if (remove(outstring))
                perror("Could not delete output file");
            return -1;
        }

        fwrite(&val, 4, 1, dest); //0 for now, but will be changed to the number of members later.
        members = 0;
        
        do 
        {
            char line[176];
            char *tok;
            
            tok = (char*) malloc(0xA0);

            src = fopen(fnd.name, "rb");
            printf("Importing %s\n", fnd.name);
            
            val = 0;
            fwrite(&val, 4, 1, dest); //Empty fields.
            fwrite(&val, 4, 1, dest); //Empty fields.

            memset(line, 0, 176);

            //File identifier
            memset(tok, 0, 0x0A);
            memset(line, 0, 176);
            fgets(line, 176, src);
            tok = strtok(line, "=");
            if (strcmp(tok, "file"))
                parsefail(fnd.name, tok);
            tok = strtok(NULL, "=");
            val = atoi(tok);
            //printf("Debug: parsed file, found %d\n", val);
            fwrite(&val, 4, 1, dest);
            
            //Auto-unlock
            memset(tok, 0, 0x0A);
            memset(line, 0, 176);
            fgets(line, 176, src);
            tok = strtok(line, "=");
            if (strcmp(tok, "auto-unlock"))
                parsefail(fnd.name, tok);
            tok = strtok(NULL, "=");
            tok[strlen(tok)-1] = '\0';
            val = strcmp(tok, "true") ? 0 : 3;
            //printf("Debug: parsed auto-unlock, found %s\n", tok);
            fwrite(&val, 4, 1, dest);
            
            //Name JP
            memset(tok, 0, 0x0A);
            memset(line, 0, 176);
            fgets(line, 176, src);
            tok = strtok(line, "=");
            if (strcmp(tok, "title_JP"))
                parsefail(fnd.name, tok);
            tok = strtok(NULL, "=");
            tok[strlen(tok)-1] = '\0';
            //printf("Debug: parsed title_JP, found %s\n", tok);
            fwrite(tok, 0x2C, 1, dest);
            
            //Name EN
            memset(tok, 0, 0x0A);
            memset(line, 0, 176);
            fgets(line, 176, src);
            tok = strtok(line, "=");
            if (strcmp(tok, "title_EN"))
                parsefail(fnd.name, tok);
            tok = strtok(NULL, "=");
            tok[strlen(tok)-1] = '\0';
            //printf("Debug: parsed title_EN, found %s\n", tok);
            fwrite(tok, 0x2C, 1, dest);
            
            //Name FR
            memset(tok, 0, 0x0A);
            memset(line, 0, 176);
            fgets(line, 176, src);
            tok = strtok(line, "=");
            if (strcmp(tok, "title_FR"))
                parsefail(fnd.name, tok);
            tok = strtok(NULL, "=");
            tok[strlen(tok)-1] = '\0';
            //printf("Debug: parsed title_FR, found %s\n", tok);
            fwrite(tok, 0x2C, 1, dest);
            
            //Name CHN
            memset(tok, 0, 0x0A);
            memset(line, 0, 176);
            fgets(line, 176, src);
            tok = strtok(line, "=");
            if (strcmp(tok, "title_CHN"))
                parsefail(fnd.name, tok);
            tok = strtok(NULL, "=");
            tok[strlen(tok)-1] = '\0';
            //printf("Debug: parsed title_CHN, found %s\n", tok);
            fwrite(tok, 0x2C, 1, dest);
            
            //Name KOR
            memset(tok, 0, 0x0A);
            memset(line, 0, 176);
            fgets(line, 176, src);
            tok = strtok(line, "=");
            if (strcmp(tok, "title_KOR"))
                parsefail(fnd.name, tok);
            tok = strtok(NULL, "=");
            tok[strlen(tok)-1] = '\0';
            //printf("Debug: parsed title_KOR, found %s\n", tok);
            fwrite(tok, 0x2C, 1, dest);
            
            //Description JP
            memset(tok, 0, 0x0A);
            memset(line, 0, 176);
            fgets(line, 176, src);
            tok = strtok(line, "=");
            if (strcmp(tok, "description_JP"))
                parsefail(fnd.name, tok);
            tok = strtok(NULL, "=");
            tok[strlen(tok)-1] = '\0';
            //printf("Debug: parsed description_JP, found %s\n", tok);
            fwrite(tok, 0xA0, 1, dest);
            
            //Description EN
            memset(tok, 0, 0x0A);
            memset(line, 0, 176);
            fgets(line, 176, src);
            tok = strtok(line, "=");
            if (strcmp(tok, "description_EN"))
                parsefail(fnd.name, tok);
            tok = strtok(NULL, "=");
            tok[strlen(tok)-1] = '\0';
            //printf("Debug: parsed description_EN, found %s\n", tok);
            fwrite(tok, 0xA0, 1, dest);
            
            //Description FR
            memset(tok, 0, 0x0A);
            memset(line, 0, 176);
            fgets(line, 176, src);
            tok = strtok(line, "=");
            if (strcmp(tok, "description_FR"))
                parsefail(fnd.name, tok);
            tok = strtok(NULL, "=");
            tok[strlen(tok)-1] = '\0';
            //printf("Debug: parsed description_FR, found %s\n", tok);
            fwrite(tok, 0xA0, 1, dest);
            
            //Description CHN
            memset(tok, 0, 0x0A);
            memset(line, 0, 176);
            fgets(line, 176, src);
            tok = strtok(line, "=");
            if (strcmp(tok, "description_CHN"))
                parsefail(fnd.name, tok);
            tok = strtok(NULL, "=");
            tok[strlen(tok)-1] = '\0';
            //printf("Debug: parsed description_CHN, found %s\n", tok);
            fwrite(tok, 0xA0, 1, dest);
            
            //Description KOR
            memset(tok, 0, 0x0A);
            memset(line, 0, 176);
            fgets(line, 176, src);
            tok = strtok(line, "=");
            if (strcmp(tok, "description_KOR"))
                parsefail(fnd.name, tok);
            tok = strtok(NULL, "=");
            tok[strlen(tok)-1] = '\0';
            //printf("Debug: parsed description_KOR, found %s\n", tok);
            fwrite(tok, 0xA0, 1, dest);
            
            //ID
            memset(tok, 0, 0x0A);
            memset(line, 0, 176);
            fgets(line, 176, src);
            tok = strtok(line, "=");
            if (strcmp(tok, "ID"))
                parsefail(fnd.name, tok);
            tok = strtok(NULL, "=");
            val = atoi(tok);
            //printf("Debug: parsed ID, found %d\n", val);
            fwrite(&val, 2, 1, dest);
            
            //Mystery
            memset(tok, 0, 0x0A);
            memset(line, 0, 176);
            fgets(line, 176, src);
            tok = strtok(line, "=");
            if (strcmp(tok, "mystery"))
                parsefail(fnd.name, tok);
            tok = strtok(NULL, "=");
            val = atoi(tok);
            //printf("Debug: parsed mystery, found %d\n", val);
            fwrite(&val, 2, 1, dest);
            
            fwrite(&members, 4, 1, dest); //In the original database the order of the songs isn't QUITE the same as their order of appearance in the list, but I'm rolling with it.
            val = 0;
            fwrite(&val, 4, 1, dest); //And god said: let there be a few zero bytes right at the end.

            fclose(src);
            members++;
        } while (!_findnext(shandle, &fnd));
        
        fseek(dest, 0, SEEK_SET);
        fwrite(&members, 4, 1, dest);
        
        printf("Done.\n");
    }
}

void usage(char *a)
{
    printf("Disgaea 5 music database converter. By BTAxis.\n");
    printf("Usage: %s [options] [input] [output]\n", a);
    printf("Options:\n\t-x: extract input archive into output directory\n\t-e: create output archive from input directory\n\n");
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

void parsefail(char* fn, char* found)
{
    printf("Error in %s: unexpected token \"%s\"", fn, found);
    exit(-1);
}