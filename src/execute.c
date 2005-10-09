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

/* $Id: execute.c,v 1.25 2005/10/05 16:06:11 timo Exp $ */

#include "bbe.h"
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <ctype.h>

/* tells if current byte should be deleted */
static int delete_this_byte;

/* tells if current block should be deleted */
static int delete_this_block;

/* tells if i or s commands are inserting bytes, meaningfull at end of the block */
static int inserting;

/* command list for write_w_command */
static struct command *current_commands;

/* commands to be executed at start of buffer */
/* note J and L must be in every string becaus ethey affect the whole block */
#define BLOCK_START_COMMANDS "DIJLFBN"

/* commands to be executed for each byte  */
#define BYTE_COMMANDS "acdirsywjplJL&|^~"

/* commands to be executed at end of buffer  */
#define BLOCK_END_COMMANDS "AJL"

/* most significant bit of byte */
#define BYTE_MASK (1 << (sizeof(unsigned char) * 8 - 1))

 
/* byte_to_string, convert byte value to visible string,
   either hex (H), decimal (D), octal (O) or ascii (A)
   */
char *
byte_to_string(unsigned char byte,char format)
{
    static char string[128];
    int i;
    int j;

    switch(format)
    {
        case 'H':
            sprintf(string,"x%02x",(int) byte);
            break;
        case 'D':
            sprintf(string,"%3d",(int) byte);
            break;
        case 'O':
            sprintf(string,"%03o",(int) byte);
            break;
        case 'A':
            sprintf(string,"%c",isprint(byte) ? byte : ' ');
            break;
        case 'B':
            i = 0;
            do
            {
                string[i] = ((BYTE_MASK >> i) & byte) ? '1' : '0';
                i++;
            } while (BYTE_MASK >> i);
            string[i] = 0;
            break;
        default:
            string[0] = 0;
            break;
    }
    return string;
}

/* convert off_t to string  */
char *
off_t_to_string(off_t number,char format)
{
    static char string[16];

    switch(format)
    {
        case 'H':
             sprintf(string,"x%llx",(long long) number);
             break;
        case 'D':
             sprintf(string,"%lld",(long long) number);
             break;
        case 'O':
             sprintf(string,"0%llo",(long long) number);
             break;
        default:
             string[0] = 0;
             break;
    }
    return string;
}




/* execute given commands */
void
execute_commands(struct command *c,char *command_letters)
{
    register int i;
    unsigned char a,b;
    unsigned char *p;
    char *str;

    while(c != NULL)
    {
        if(strchr(command_letters,c->letter) !=  NULL)
        {
            switch(c->letter)
            {
                case 'A':
                case 'I':
                    write_buffer(c->s1,c->s1_len);
                    break;
                case 'd':
                    if(c->rpos || c->offset == in_buffer.block_offset) 
                    {
                        if(c->rpos < c->count)
                        {
                            delete_this_byte = 1;
                            c->rpos++;
                        } else
                        {
                            c->rpos = 0;
                        }
                    }
                    break;
                case 'D':
                    if(c->offset == in_buffer.block_num || c->offset == 0) delete_this_block = 1;
                    break;
                case 'i':
                    if(c->offset == in_buffer.block_offset && !c->rpos) 
                    {
                        c->rpos = 1;
                        inserting = 1;
                        break;
                    } 
                    if(c->rpos > 0 && c->rpos <= c->s1_len)
                    {
                        if(c->rpos <= c->s1_len)
                        {
                            put_byte(c->s1[c->rpos - 1]);
                            if(c->rpos < c->s1_len) inserting = 1;
                        }
                        c->rpos++;
                    }
                    break;
                case 'r':
                    if(in_buffer.block_offset >= c->offset &&
                       in_buffer.block_offset < c->offset + c->s1_len)
                    {
                        put_byte(c->s1[in_buffer.block_offset - c->offset]);
                    }
                    break;
                case 's':
                    if(c->rpos)
                    {
                        if(c->rpos < c->s1_len && c->rpos < c->s2_len)
                        {
                            put_byte(c->s2[c->rpos]);
                        } else if (c->rpos < c->s1_len && c->rpos >= c->s2_len)
                        {
                            delete_this_byte = 1;
                        } else if(c->rpos >= c->s1_len && c->rpos < c->s2_len)
                        {
                            put_byte(c->s2[c->rpos]);
                        } 

                        if(c->rpos >= c->s1_len - 1 && c->rpos < c->s2_len - 1)
                        {
                            inserting = 1;
                        }

                        c->rpos++;
                        if(c->rpos >= c->s1_len && c->rpos >= c->s2_len)
                        {
                            c->rpos = 0;
                        }
                        break;
                    }
                    if(delete_this_byte) break;
                    p = out_buffer.write_pos;
                    i = 0;
                    while(*p == c->s1[i] && i < c->s1_len)
                    {
                        if(p == out_buffer.write_pos) p = read_pos();
                        if(p == block_end_pos() && c->s1_len - 1 > i) break;
                        i++;
                        p++;
                    }
                    if(i == c->s1_len) 
                    {
                        if(c->s1_len > 1 || c->s2_len > 1) c->rpos = 1;
                        if(c->s2_len) 
                        {
                            put_byte(c->s2[0]);
                            if(c->s1_len == 1 && c->s2_len > 1) inserting = 1;
                        } else
                        {
                            delete_this_byte = 1;
                        }
                    }
                    break;
                case 'y':
                    i = 0;
                    while(c->s1[i] != *out_buffer.write_pos && i < c->s1_len) i++;
                    if(c->s1[i] == *out_buffer.write_pos && i < c->s1_len) put_byte(c->s2[i]);
                    break;
                case 'c':
                    switch(c->s1[0])
                    {
                        case 'A':                   // from ascii
                            switch(c->s1[3])
                            {
                                case 'B':           // to bcd
                                    if(c->rpos || (last_byte() && out_buffer.block_offset == 0))     // skip first nibble
                                    {
                                        c->rpos = 0;
                                        if(last_byte())  // unless last byte of block
                                        {
                                            if(*out_buffer.write_pos >= '0' && *out_buffer.write_pos <= '9') 
                                            {
                                                a = *out_buffer.write_pos - '0';
                                                a = (a << 4) & 0xf0;
                                                b = 0x0f;
                                                *out_buffer.write_pos = a | b;
                                            }
                                        }
                                        break;
                                    }
                                    if(out_buffer.block_offset == 0 || delete_this_byte) break;
                                    if((out_buffer.write_pos[-1] >= '0' && out_buffer.write_pos[-1] <= '9'))
                                    {
                                        a = out_buffer.write_pos[-1] - '0';
                                        a = (a << 4) & 0xf0;
                                        if(*out_buffer.write_pos >= '0' && *out_buffer.write_pos <= '9')
                                        {
                                            b = *out_buffer.write_pos - '0';
                                            b &= 0x0f;
                                            delete_this_byte = 1;
                                            c->rpos = 1;
                                        } else
                                        {
                                            b = 0x0f;
                                            if(*out_buffer.write_pos == 'F' || *out_buffer.write_pos == 'f') delete_this_byte=1;
                                        }
                                        out_buffer.write_pos[-1] = a | b;
                                    }
                                    break;
                            }
                            break;
                        case 'B':               // from bcd
                            switch(c->s1[3])
                            {
                                case 'A':       // to ascii
                                    if(((*out_buffer.write_pos >> 4) & 0x0f) <= 9 && 
                                            ((*out_buffer.write_pos & 0x0f) <= 9 || (*out_buffer.write_pos & 0x0f) == 0x0f))
                                    {
                                        a = (*out_buffer.write_pos >> 4) & 0x0f;
                                        b = *out_buffer.write_pos & 0x0f;
                                        *out_buffer.write_pos = '0' + a;
                                        if(!delete_this_byte) 
                                        {
                                            write_next_byte();
                                            if(b == 0x0f)
                                            {
                                                *out_buffer.write_pos = 'F';
                                            } else
                                            {
                                                *out_buffer.write_pos = '0' + b;
                                            }
                                        }
                                    }
                                    break;
                            }
                            break;
                    }
                    break;
                case 'j':
                    if(in_buffer.block_offset < c->count)
                    {
                        while(c->next != NULL) c = c->next;     // skip rest of commands
                    }
                    break;
                case 'J':
                    if(in_buffer.block_num <= c->count)
                    {
                        while(c->next != NULL) c = c->next;     // skip rest of commands
                    }
                    break;
                case 'l':
                    if(in_buffer.block_offset >= c->count)
                    {
                        while(c->next != NULL) c = c->next;     // skip rest of commands
                    }
                    break;
                case 'L':
                    if(in_buffer.block_num > c->count)
                    {
                        while(c->next != NULL) c = c->next;     // skip rest of commands
                    }
                    break;
                case 'p':
                    if (delete_this_byte) break;
                    i = 0;
                    a = *out_buffer.write_pos;
                    while(i < c->s1_len)
                    {
                        str = byte_to_string(a,c->s1[i]);
                        write_string(str);
                        i++;
                        if (i < c->s1_len) 
                        {
                            put_byte('-');
                            write_next_byte();
                        }
                    }
                    put_byte(' ');
                    break;
                case 'F':
                    str = off_t_to_string(in_buffer.stream_offset + (off_t) (in_buffer.read_pos-in_buffer.buffer),c->s1[0]);
                    write_string(str);
                    put_byte(':');
                    write_next_byte();
                    break;
                case 'B':
                    str = off_t_to_string(in_buffer.block_num,c->s1[0]);
                    write_string(str);
                    put_byte(':');
                    write_next_byte();
                    break;
                case 'N':
                    write_string(get_current_file());
                    put_byte(':');
                    write_next_byte();
                    break;
                case '&':
                    put_byte(*out_buffer.write_pos & c->s1[0]);
                    break;
                case '|':
                    put_byte(*out_buffer.write_pos | c->s1[0]);
                    break;
                case '^':
                    put_byte(*out_buffer.write_pos ^ c->s1[0]);
                    break;
                case '~':
                    put_byte(~*out_buffer.write_pos);
                    break;
                case 'w':
                    break;
            }
        }
        c = c->next;
    }
}

/* write w command, will be called when output_buffer is written, same will be written to w-command files */
void
write_w_command(unsigned char *buf,size_t length)
{
    struct command *c;

    c = current_commands;

    while(c != NULL)
    {
        if(c->letter == 'w')
        {
            if(fwrite(buf,1,length,c->fd) != length) panic("Cannot write to file",c->s1,strerror(errno));
        }
        c = c->next;
    }
}



/* init_commands, initialize those wich need it, currently w - open file and rpos=0 for all */
void
init_commands(struct command *c)
{
    while(c != NULL)
    {
        switch(c->letter)
        {
            case 'w':
                c->fd = fopen(c->s1,"w");
                if(c->fd == NULL) panic("Cannot open file for writing",c->s1,strerror(errno));
                break;
        }
        c = c->next;
    }
}


/* close_commands, close those wich need it, currently w - close file */
void
close_commands(struct command *c)
{
    while(c != NULL)
    {
        switch(c->letter)
        {
            case 'w':
                if(fclose(c->fd) != 0) panic("Error in closing file",c->s1,strerror(errno));
                break;
        }
        c = c->next;
    }
}

/* reset the rpos counter for next block, in case block was shorter eg. delete count */
inline void
reset_rpos(struct command *c)
{
    while(c != NULL)
    {
        c->rpos = 0;
        c = c->next;
    }
}



/* main execution loop */
void
execute_program(struct command *c)
{
    int block_end;

    current_commands = c;

    while(find_block())
    {
        reset_rpos(c);
        delete_this_block = 0;
        out_buffer.block_offset = 0;
        execute_commands(c,BLOCK_START_COMMANDS);
        do
        {
            set_cycle_start();
            delete_this_byte = 0;
            inserting = 0;
            block_end = last_byte();
            put_byte(read_byte());     // as default write current byte from input
            execute_commands(c,BYTE_COMMANDS);
            if(!delete_this_byte && !delete_this_block)
            {
               write_next_byte();           // advance the write pointer if byte is not marked for del
            }
            if(!block_end && !inserting) get_next_byte();
        } while (!block_end || inserting);
        execute_commands(c,BLOCK_END_COMMANDS);
        flush_buffer();
    }
    close_output_stream();
}
