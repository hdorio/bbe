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

/* $Id: bbe.h,v 1.28 2005/11/11 13:06:53 timo Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_FEATURES_H
#include <features.h>
#endif

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#ifdef HAVE_ERROR_H
#include <error.h>
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif 

#include <stdio.h>

#ifndef HAVE_OFF_T
#define long int off_t
#endif

/* Types */

/* Constants */
/* exit values */
#define EXIT_FAILURE 1
#define EXIT_SUCCESS 0

/* Input buffer size */
#define INPUT_BUFFER_LOW (16*1024)
#define INPUT_BUFFER_SIZE (16*INPUT_BUFFER_LOW)
#define INPUT_BUFFER_SAFE (INPUT_BUFFER_SIZE - INPUT_BUFFER_LOW)

/* Output buffer size*/
#define OUTPUT_BUFFER_LOW INPUT_BUFFER_LOW
#define OUTPUT_BUFFER_SIZE (16*OUTPUT_BUFFER_LOW)
#define OUTPUT_BUFFER_SAFE (OUTPUT_BUFFER_SIZE - OUTPUT_BUFFER_LOW)

/* block types */
#define BLOCK_START_M 1
#define BLOCK_START_S 2
#define BLOCK_STOP_M  4
#define BLOCK_STOP_S  8

/* structs */

/* Block definition */
struct block {
    int type;
    union 
    {
        off_t N;
        struct {
            unsigned char *string;
            off_t length;
        } S;
    } start;
    union 
    {
        off_t M;
        struct {
            unsigned char *string;
            off_t length;
        } S;
    } stop;
};

/* Commands */

struct command_list {
    char letter;            // command letter (D,A,s,..)
    off_t offset;           // n for D,r,i and d commands
    off_t count;              // count for d command
    unsigned char *s1;      // string for A,I,r,i,s,w and y commands
    off_t s1_len;
    unsigned char *s2;      // replace for s and dest for y 
    off_t s2_len;
    int rpos;               // replace position for s,r and y
    off_t fpos;             // found pos for s-command
    FILE *fd;               // stream for w command
    struct command_list *next;
};

struct commands {
    struct command_list *block_start;
    struct command_list *byte;
    struct command_list *block_end;
};

/* in/out files */
struct io_file {
    char *file;
    int fd;
    off_t start_offset;
    struct io_file *next;
};

/* input buffer */
struct input_buffer {
    unsigned char *buffer;       // buffer to be malloced
    unsigned char *read_pos;     // current read position
    unsigned char *low_pos;      // low water mark
    unsigned char *block_end;    // end of current block (if in buffer)
    unsigned char *stream_end;   // end of stream (if in buffer)
    off_t stream_offset;         // stream offset (at the beginning of buffer) current offset: offset + (read_pos - buffer)
    off_t block_offset;          // block offset (start = 0) number of bytes read at position read_pos
    off_t block_num;             // number of current block, first = 1
};

/* output buffer */
struct output_buffer {
    unsigned char *buffer;
    unsigned char *end;
    unsigned char *write_pos;    // current write psotion;
    unsigned char *low_pos;      // low water mark
    off_t block_offset;          // block offset (start = 0) number of bytes written at position write_pos
};
    


/* function prototypes */
extern void 
panic(char *msg,char *info,char *syserror);

extern void *
xmalloc (size_t size);

extern void
set_output_file(char *file);

extern void
set_input_file(char *file);

extern void
init_buffer();

extern inline unsigned char  
read_byte();

extern inline int 
get_next_byte();

extern void
mark_block_end();

extern int
find_block();

extern inline int
last_byte();

extern void
write_buffer(unsigned char *buf,off_t length);

extern inline void
put_byte(unsigned char byte);

extern inline void
write_next_byte();
 
extern void
flush_buffer();

extern void
init_commands(struct commands *c);

extern void
close_commands(struct commands *c);

extern void
close_output_stream();

extern void
write_w_command(unsigned char *buf,size_t length);

extern void
execute_program(struct commands *c);

extern void
write_string(char *string);

extern char *
get_current_file(void);

extern inline unsigned char *
read_pos();

extern inline unsigned char *
block_end_pos();

extern char *
xstrdup(char *str);

/* global variables */
extern struct block block;
extern struct command *commands;
extern struct io_file out_stream;
extern struct input_buffer in_buffer;
extern struct output_buffer out_buffer;
extern int output_only_block;
