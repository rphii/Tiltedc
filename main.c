#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>

typedef enum
{
    OS_LINUX,
    OS_WINDOWS,
    OS_MAC,
}
Os;

typedef enum
{
    CMD_NONE,
    CMD_C2TC,
    CMD_TC2C,
}
Cmd;

#define OS_TYPE OS_WINDOWS  // DEFINE YOUR OS HERE :)

void error_args(char **argv, int i, int j, char *msg)
{
    printf("Error on argument %d at position %d\n", i, j);
    printf("Here: %s\n", argv[i]);
    for(int i = 0; i < j + 6; i++) printf(" ");
    printf("^ %s\n", msg);
}

size_t file_read(char *filename, char **dump)
{
    if(!filename || !dump) return 0;
    FILE *file = fopen(filename, "rb");
    if(!file) return 0;

    // get file length 
    fseek(file, 0, SEEK_END);
    size_t bytes_file = ftell(file);
    fseek(file, 0, SEEK_SET);

    // allocate memory
    if(!(*dump = realloc(*dump, bytes_file + 1))) return 0;

    // read file
    size_t bytes_read = fread(*dump, 1, bytes_file, file);
    if(bytes_file != bytes_read) return 0;
    (*dump)[bytes_read] = 0;

    // close file
    fclose(file);
    return bytes_read;
}

size_t file_write(char *filename, char *dump, size_t len)
{
    // open file
    FILE *file = fopen(filename, "wb");
    if(!file) return 0;

    // write file
    uint32_t result = fwrite(dump, sizeof(char), len, file);

    // close file
    fclose(file);

    return result;
}

bool append(char **dump, size_t *len, char c, int batch)
{
    if(!dump || !len) return false;
    // check for memory
    if(!(*len % batch)) *dump = realloc(*dump, (1 + *len / batch) * batch);
    if(!*dump) return false; 
    // append
    (*dump)[*len] = c;
    (*len)++;
    return true;
}

int isnewline(char *c, size_t i, size_t len)
{
    if(!c || i > len) return 0;
    switch(OS_TYPE)
    {
        case OS_WINDOWS:
            if(c[i] == '\r' && i + 1 < len && c[i + 1] == '\n') return 2;
            break;
        case OS_LINUX:
            if(c[i] == '\n') return 1;
            break;
        case OS_MAC:
            if(c[i] == '\r') return 1;
            if(c[i] == '\n') return 1;
            break;
        default:
            break;
    }
    return 0;
}

char getcxy(char *dump, size_t dlen, int *nl, int nlcount, int x, int y)
{
    if(!dump || !nl) return 0;
    if(y > nlcount) return 0;
    x += (nl[y] + isnewline(dump, nl[y], dlen));
    if(y < nlcount && x >= nl[y + 1]) return 0;
    if(x >= dlen) return 0;
    return dump[x];
}

void c2tc(char *file_in, char *file_out)
{
    char *din = 0;  // dump in
    char *dout = 0; // dump out
    size_t dinlen = file_read(file_in, &din);
    size_t doutlen = 0;

    if(!dinlen)
    {
        printf("Could not open/read file.\n");
        return;
    }

    int nlcount = 1;    // DO start from 1
    // count lines
    for(size_t i = 0; i < dinlen; i++)
    {
        if(isnewline(din, i, dinlen)) nlcount++;
    }
    // count lines => store in array
    int *nl = malloc(nlcount * sizeof(int));
    if(!nl)
    {
        printf("Failed to malloc.\n");
        return;
    }
    for(int i = 0; i < nlcount; i++) nl[i] = 0;
    nlcount = 1;
    for(size_t i = 0; i < dinlen; i++)
    {
        if(isnewline(din, i, dinlen)) nl[nlcount++] = i;
    }

    // get the longest line length
    int longest = 0;
    for(int i = 0; i + 1 < nlcount; i++)
    {
        int len = nl[i + 1] - nl[i] - isnewline(din, nl[i], dinlen);
        if(len > longest) longest = len;
    }
    // append
    bool result = true;
    for(int x = 0; x < longest; x++)
    {
        int pending = 0;    // pending spaces to be added
        for(int y = 0; y < nlcount; y++)
        {
            char c = getcxy(din, dinlen, nl, nlcount, x, y);
            if(!c || c == '\t' || c == ' ') pending++;
            else
            {
                for(int k = 0; k < pending; k++)
                {
                    result &= append(&dout, &doutlen, ' ', 64);
                }
                pending = 0;
                result &= append(&dout, &doutlen, c, 64);
            }
        }
        // append newline
        switch(OS_TYPE)
        {
            case OS_WINDOWS:
                result &= append(&dout, &doutlen, '\r', 64);
            case OS_LINUX:
            case OS_MAC:
                result &= append(&dout, &doutlen, '\n', 64);
                break;
            default:
                break;
        }
    }
    if(result) file_write(file_out, dout, doutlen);
    free(nl);
    free(din);
}

int main(int argc, char **argv)
{
    Cmd cmd = CMD_NONE;
    char *file_in = 0;
    char *file_out = 0;

    // commands:
    // file.c -t file.tc   convert c to tc
    // file.tc -c file.c   convert tc to c
    // (they do the same)

    for(int i = 1; i < argc; i++)
    {
        // check commands
        if(cmd) file_out = argv[i];
        else if(file_in)
        {
            if(!strcmp(argv[i], "-c")) cmd = CMD_TC2C;
            else if(!strcmp(argv[i], "-t")) cmd = CMD_C2TC;
            else 
            {
                error_args(argv, i, 1, "wrong command.");
                return -1;
            }
        }
        else file_in = argv[i];
        // run command
        if(file_in && cmd && file_out)
        {
            switch(cmd)
            {
                case CMD_TC2C:
                    c2tc(file_in, file_out);
                    break;
                case CMD_C2TC:
                    c2tc(file_in, file_out);
                    break;
                default:
                    break;
            }
            // reset
            cmd = 0;
            file_in = 0;
            file_out = 0;
        }
    }
    return 0;
}
