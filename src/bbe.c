/*
 *    bbe - Binary block editor
 *
 *    Copyright (C) 2005 Timo Savinen
 *    This file is part of bbe.
 * 
 *    bbe is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    bbe is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with bbe; if not, write to the Free Software
 *    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

/* $Id: bbe.c,v 1.23 2005/09/14 15:48:52 timo Exp $ */

#include "bbe.h"
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef PACKAGE
static char *program = PACKAGE;
#else
static char *program = "bbe";
#endif

#ifdef VERSION
static char *version = VERSION;
#else
static char *version = "0.1.0";
#endif

#ifdef PACKAGE_BUGREPORT
static char *email_address = PACKAGE_BUGREPORT;
#else
static char *email_address = "tjsa@iki.fi";
#endif



struct block block;
struct command *commands = NULL;
char *panic_info = NULL;

char *convert_strings[] = {
    "BCDASC",
    "ASCBCD",
          "",
};

static char short_opts[] = "b:e:f:o:?V";

static struct option long_opts[] = {
    {"block",1,NULL,'b'},
    {"expression",1,NULL,'e'},
    {"file",1,NULL,'f'},
    {"output",1,NULL,'o'},
    {"help",0,NULL,'?'},
    {"version",0,NULL,'V'},
    {NULL,0,NULL,0}
};

void
panic(char *msg,char *info,char *syserror)
{
    if(panic_info != NULL) fprintf(stderr,"%s: %s",program,panic_info);

    if (info == NULL && syserror == NULL)
    {
        fprintf(stderr,"%s: %s\n",program,msg);
    } else if(info != NULL && syserror == NULL)
    {
        fprintf(stderr,"%s: %s: %s\n",program,msg,info);
    } else if(info != NULL && syserror != NULL)
    {
        fprintf(stderr,"%s: %s: %s; %s\n",program,msg,info,syserror);
    } else if(info == NULL && syserror != NULL)
    {
        fprintf(stderr,"%s: %s; %s\n",program,msg,syserror);
    }
    exit(EXIT_FAILURE);
}

/* parse a long int, can start with n (dec), x (hex), 0 (oct) */
off_t
parse_long(char *long_int)
{
    long long int l;
    char *scan = long_int;
    char type='d';           // others are x and o


    if(*scan == '0')
    {
        type = 'o';
        scan++;
        if(*scan == 'x' || *scan == 'X') {
            type = 'x';
            scan++;
        }
    }

    while(*scan != 0)
    {
        switch(type)
        {
            case 'd':
                if(!isdigit(*scan)) panic("Error in number",long_int,NULL);
                break;
            case 'o':
                if(!isdigit(*scan) || *scan > '8') panic("Error in number",long_int,NULL);
                break;
            case 'x':
                if(!isxdigit(*scan)) panic("Error in number",long_int,NULL);
                break;
        }
        scan++;
    }

    if (sscanf(long_int,"%lli",&l) != 1)
    {
        panic("Error in number",long_int,NULL);
    }
    return (off_t) l;
}

/* parse a string, string can contain \n, \xn, \0n and \\
   escape codes. memory will be allocated */
unsigned char *
parse_string(char *string,off_t *length)
{
    char *p;
    int j,k,i = 0;
    int min_len;
    unsigned char buf[INPUT_BUFFER_LOW+1];
    char num[5];
    unsigned char *ret;

    p = string;

    while(*p != 0)
    {
        if(*p == '\\')
        {
            p++;
            if(*p == '\\')
            {
                num[i] = *p++;
            } else
            {
                j = 0;
                switch(*p)
                {
                    case 'x':
                    case 'X':
                        num[j++] = '0';
                        num[j++] = *p++;
                        while(isxdigit(*p) && j < 4) num[j++] = *p++;
                        min_len=3;
                        break;
                    case '0':
                        while(isdigit(*p) && *p <= '8' && j < 4) num[j++] = *p++;
                        min_len=1;
                        break;
                    default:
                        while(isdigit(*p) && j < 3) num[j++] = *p++;
                        min_len=1;
                        break;
                }
                num[j] = 0;
                if (sscanf(num,"%i",&k) != 1 || j < min_len)
                {
                    panic("Syntax error in escape code",string,NULL);
                }
                if (k < 0 || k > 255)
                {
                    panic("Escape code not in range (0-255)",string,NULL);
                }
                buf[i] = (unsigned char) k;
            }
        } else
        {
            buf[i] = (unsigned char) *p++;
        }
        if(i > INPUT_BUFFER_LOW)
        {
            panic("string too long",string,NULL);
        }
        i++;
    }
    if(i)       
    {
        ret = (unsigned char *) xmalloc(i);
        memcpy(ret,buf,i);
    } else
    {
        ret = NULL;
    }
    *length = i;
    return ret;
}


/* parse a block definition and save it to block */
static void
parse_block(char *bs)
{
    char slash_char;
    char *p = bs;
    int i = 0;
    char *buf;

    if(strlen(bs) > (2*4*INPUT_BUFFER_LOW))
    {
        panic("Block definition too long",NULL,NULL);
    }

    buf=xmalloc(2*4*INPUT_BUFFER_LOW);

    if (*p == ':')
    {
        block.start.S.length = 0;
        block.type |= BLOCK_START_S;
    } else
    {
        if(*p == 'x' || *p == 'X' || isdigit(*p))
        {
            switch(*p)
            {
                case 'x':
                case 'X':
                    buf[i++] = '0';
                    buf[i++] = *p++;
                    while(isxdigit(*p)) buf[i++] = *p++;
                    break;
                case '0':
                    while(isdigit(*p) && *p <= '8') buf[i++] = *p++;
                    break;
                default:
                    while(isdigit(*p)) buf[i++] = *p++;
                    break;
            }

            buf[i] = 0;
            block.start.N = parse_long(buf);
            block.type |= BLOCK_START_M;
        } else                                // string start
        {
            slash_char = *p;
            p++;
            while(*p != slash_char && *p != 0) buf[i++] = *p++;
            if (*p == slash_char) p++;
            buf[i] = 0;
            block.start.S.string = parse_string(buf,&block.start.S.length);
            block.type |= BLOCK_START_S;
        }
    } 

    if (*p != ':')
    {
        panic("Error in block definition",bs,NULL);
    }

    p++;

    if (*p == 0)
    {
        block.stop.S.length = 0;
        block.type |= BLOCK_STOP_S;
    } else
    { 
        i = 0;
        if (*p == 'x' || *p == 'X' || isxdigit(*p))
        {
            switch(*p)
            {
                case 'x':
                case 'X':
                    buf[i++] = '0';
                    buf[i++] = *p++;
                    while(isxdigit(*p)) buf[i++] = *p++;
                    break;
                case '0':
                    while(isdigit(*p) && *p <= '8') buf[i++] = *p++;
                    break;
                default:
                    while(isdigit(*p)) buf[i++] = *p++;
                    break;
            } 
            buf[i] = 0;
            block.stop.M = parse_long(buf);
            if(block.stop.M == 0) panic("Block length must be greater than zero",NULL,NULL);
            block.type |= BLOCK_STOP_M;
        } else
        {
            if(*p == '$')
            {
                block.stop.S.length = 0;
                p++;
            } else
            {
                slash_char = *p;
                p++;
                while(*p != slash_char && *p != 0) buf[i++] = *p++;
                if (*p == slash_char)
                {
                    p++;
                } else
                {
                    panic("syntax error in block definition",bs,NULL);
                }
                buf[i] = 0;
                block.stop.S.string = parse_string(buf,&block.stop.S.length);
                block.type |= BLOCK_STOP_S;
            }
        }
    } 
    if (*p != 0)
    {
        panic("syntax error in block definition",bs,NULL);
    }
    free(buf);
}

/* parse one command, commands are in list pointed by commands */
void
parse_command(char *command_string)
{
    struct command *curr,*new;
    char *c,*p,*buf;
    char *token[10];
    char slash_char;
    int i,j;

   
    p = command_string;
    while(isspace(*p)) p++;              // remove leading spaces
    c = strdup(p);
    if(c == NULL) panic("Out of memory",NULL,NULL);

    i = 0;
    token[i] = strtok(c," \t\n");
    i++;
    while(token[i - 1] != NULL && i < 10) token[i++] = strtok(NULL," \t\n");
    i--;

    if (token[0] == NULL ) return;      // empty line
    if (*token[0] == '#') return;       // comment

    curr = commands;
    if (curr != NULL)
    {
        while(curr->next != NULL)  curr = curr->next;
    }
    new = xmalloc(sizeof(struct command));
    new->next = NULL;
    if(commands == NULL)
    {
        commands = new;
    } else
    {
        curr->next = new;
    }
    

    new->letter = token[0][0];
    switch(new->letter)
    {
        case 'D':
            if(i < 1 || i > 2 || strlen(token[0]) > 1) panic("Error in command",command_string,NULL);
            if(i == 2) 
            {
                new->offset = parse_long(token[1]);
                if(new->offset < 1) panic("n for D-command must be at least 1",NULL,NULL);
            } else
            {
                new->offset = 0;
            }
            break;
        case 'A':
        case 'I':
            if(i != 2 || strlen(token[0]) > 1) panic("Error in command",command_string,NULL);
            new->s1 = parse_string(token[1],&new->s1_len);
            break;
        case 'w':
            if(i != 2 || strlen(token[0]) > 1) panic("Error in command",command_string,NULL);
            new->s1 = strdup(token[1]);
            break;
        case 'j':
        case 'J':
            if(i != 2 || strlen(token[0]) > 1) panic("Error in command",command_string,NULL);
            new->count = parse_long(token[1]);
            break;
        case 'l':
        case 'L':
            if(i != 2 || strlen(token[0]) > 1) panic("Error in command",command_string,NULL);
            new->count = parse_long(token[1]);
            break;
        case 'r':
        case 'i':
            if(i != 3 || strlen(token[0]) > 1) panic("Error in command",command_string,NULL);
            new->offset = parse_long(token[1]);
            new->s1 = parse_string(token[2],&new->s1_len);
            break;
        case 'd':
            if(i < 2 || i > 3 || strlen(token[0]) > 1) panic("Error in command",command_string,NULL);
            new->offset = parse_long(token[1]);
            new->count = 1;
            if(i == 3) new->count = parse_long(token[2]);
            break;
        case 'c':
            if(i != 3 || strlen(token[1]) != 3 || strlen(token[2]) != 3 || strlen(token[0]) > 1) panic("Error in command",command_string,NULL);
            new->s1 = xmalloc(strlen(token[1]) + strlen(token[2]) + 2);
            strcpy(new->s1,token[1]);
            strcat(new->s1,token[2]);
            j = 0;
            while(new->s1[j] != 0) {
                new->s1[j] = toupper(new->s1[j]);
                j++;
            }
            j = 0;
            while(*convert_strings[j] != 0 && strcmp(convert_strings[j],new->s1) != 0) j++;
            if(*convert_strings[j] == 0) panic("Unknown conversion",command_string,NULL);
            break;
        case 's':
        case 'y':
            if(strlen(command_string) < 4) panic("Error in command",command_string,NULL);

            buf=xmalloc((4*INPUT_BUFFER_LOW) + 1);

            slash_char = command_string[1];
            p = command_string;
            p += 2;
            j = 0;
            while(*p != 0 && *p != slash_char && j < 4*INPUT_BUFFER_LOW) buf[j++] = *p++;
            if(*p != slash_char) panic("Error in command",command_string,NULL);
            buf[j] = 0;
            new->s1 = parse_string(buf,&new->s1_len);
            if(new->s1_len > INPUT_BUFFER_LOW) panic("String in command too long",command_string,NULL);
            if(new->s1_len == 0) panic("Error in command",command_string,NULL);

            p++;

            j = 0;
            while(*p != 0 && *p != slash_char && j < 4*INPUT_BUFFER_LOW) buf[j++] = *p++;
            buf[j] = 0;
            if(*p != slash_char) panic("Error in command",command_string,NULL);
            new->s2 = parse_string(buf,&new->s2_len);
            if(new->s2_len > INPUT_BUFFER_LOW) panic("String in command too long",command_string,NULL);

            if(new->letter == 'y' && new->s1_len != new->s2_len) panic("Strings in y-command must have equal length",command_string,NULL);
            free(buf);
            break;
        default:
            panic("Unknown command",command_string,NULL);
            break;
    }
    free(c);
}

/* read commands from file */
void
parse_command_file(char *file)
{
    FILE *fp;
    char *line;
    char info[1024];
    size_t line_len = 1024;
    int line_no = 0;

    line = xmalloc(line_len);

    fp = fopen(file,"r");
    if (fp == NULL) panic("Error in opening file",file,strerror(errno));

    while(getline(&line,&line_len,fp) != -1) 
    {
        line_no++;
        sprintf(info,"Error in file '%s' in line %d\n",file,line_no);
        panic_info=info;
        parse_command(line);
    }

    free(line);
    fclose(fp);
    panic_info=NULL;
}

void
help(FILE *stream)
{
    fprintf(stream,"Usage: %s [OPTION]...\n\n",program);
    fprintf(stream,"-b, --block=BLOCK\n");
    fprintf(stream,"\t\tBlock definition.\n");
    fprintf(stream,"-e, --expression=COMMAND\n");
    fprintf(stream,"\t\tAdd command to the commands to be executed.\n");
    fprintf(stream,"-f, --file=script-file\n");
    fprintf(stream,"\t\tAdd commands from script-file to the commands to be executed.\n");
    fprintf(stream,"-o, --output=name\n");
    fprintf(stream,"\t\tWrite output to name instead of standard output.\n");
    fprintf(stream,"-?, --help\n");
    fprintf(stream,"\t\tDisplay this help and exit\n");
    fprintf(stream,"-V, --Version\n");
    fprintf(stream,"\t\tShow version and exit\n");
    fprintf(stream,"\nAll remaining arguments are names of input files;\n");
    fprintf(stream,"if no input files are specified, then the standard input is read.\n");
    fprintf(stream,"\nSend bug reports to %s\n",email_address);
}

void 
usage(int opt)
{
    printf("Unknown option '-%c'\n",(char) opt);
    help(stderr);
}

void
print_version()
{
    printf("%s version %s\n",program,version);
    printf("Copyright (c) 2005 Timo Savinen\n\n");
    printf("This is free software; see the source for copying conditions.\n");
    printf("There is NO warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n");
}


int
main (int argc, char **argv)
{
    int opt;

    block.type = 0;
    while ((opt = getopt_long(argc,argv,short_opts,long_opts,NULL)) != -1)
    {
        switch(opt)
        {
            case 'b':
                if(block.type) panic("Only one -b option allowed",NULL,NULL);
                parse_block(optarg);
                break;
            case 'e':
                parse_command(optarg);
                break;
            case 'f':
                parse_command_file(optarg);
                break;
            case 'o':
                set_output_file(optarg);
                break;
            case '?':
                help(stdout);
                exit(EXIT_SUCCESS);
                break;
            case 'V':
                print_version();
                exit(EXIT_SUCCESS);
                break;
            default:
                usage(opt);
                exit(EXIT_FAILURE);
                break;
        }
    }
    if(!block.type) parse_block("0:$");
    if(out_stream.file == NULL) set_output_file(NULL);

    if(optind < argc)
    {
        while(optind < argc) set_input_file(argv[optind++]);
    } else
    {
        set_input_file(NULL);
    }

    init_buffer();
    init_commands(commands);
    execute_program(commands);
    close_commands(commands); 
    exit(EXIT_SUCCESS);
}