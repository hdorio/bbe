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

/* $Id: buffer.c,v 1.37 2006-11-02 17:11:36 timo Exp $ */

#include "bbe.h"
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

/* output file */
struct io_file out_stream;

/* list of input files, points to current file */
struct io_file *in_stream = NULL;
struct io_file *in_stream_start = NULL;

/* input buffer */
struct input_buffer in_buffer;

/* output buffer */
struct output_buffer out_buffer;

/* open the output file */
void 
set_output_file(char *file)
{
    if (out_stream.file != NULL) panic("Only one output file can be defined",NULL,NULL);

    if(file == NULL)
    {
        out_stream.fd = STDOUT_FILENO;
        out_stream.file = "(stdout)";
    } else
    {
        out_stream.fd = open(file,O_WRONLY | O_CREAT | O_TRUNC,S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
        if(out_stream.fd == -1) panic("Cannot open for writing",file,strerror(errno));
        out_stream.file = xstrdup(file);
    }
}

/* write to output stream from arbitrary buffer */
void
write_output_stream(unsigned char *buffer, ssize_t length)
{
    if(write(out_stream.fd,buffer,length) == -1) panic("Error writing to",out_stream.file,strerror(errno));
}


/* open a input file and put it in input file list */
void
set_input_file(char *file)
{
    struct io_file *new,*curr;

    new = xmalloc(sizeof(struct io_file));
    new->next = NULL;

    if(in_stream ==  NULL)
    {
        in_stream = new;
        in_stream_start = in_stream;
    } else
    {
        curr = in_stream;
        while(curr->next != NULL) 
        {
            curr = curr->next;
        }
        curr->next = new;
    }

    new->start_offset = (off_t) 0;
    if(file[0] == '-' && file[1] == 0)
    {
        new->fd = STDIN_FILENO;
        new->file = "(stdin)";
    } else
    {
        new->fd = open(file,O_RDONLY);
        if(new->fd == -1) panic("Cannot open file for reading",file,strerror(errno));
        new->file = xstrdup(file);
    }
}

/* return the name of current input file */
char *
get_current_file(void)
{
    struct io_file *f = in_stream_start;
    struct io_file *prev;
    off_t current_offset = in_buffer.stream_offset + (off_t) (in_buffer.read_pos-in_buffer.buffer);

    if(f == NULL) return "";

    while(f != NULL)
    {
        prev = f;
        f = f->next;
        if(f != NULL && (f->start_offset == (off_t) 0 || f->start_offset > current_offset)) 
        {
            f = NULL;
        }
    }
    return prev->file;
}



/* initialize in and out buffers */
void
init_buffer()
{
    in_buffer.buffer = xmalloc(INPUT_BUFFER_SIZE);
    in_buffer.read_pos = NULL;
    in_buffer.stream_end = NULL;
    in_buffer.low_pos = in_buffer.buffer + INPUT_BUFFER_SAFE;
    in_buffer.block_num = 0;

    out_buffer.buffer = xmalloc(OUTPUT_BUFFER_SIZE);
    out_buffer.end = out_buffer.buffer + OUTPUT_BUFFER_SIZE;
    out_buffer.write_pos = out_buffer.buffer;
    out_buffer.low_pos = out_buffer.buffer + OUTPUT_BUFFER_SAFE;
}

ssize_t
read_input_stream()
{
    ssize_t read_count,last_read,to_be_read,to_be_saved;
    unsigned char *buffer_write_pos;

    if(in_buffer.stream_end != NULL) return (ssize_t) 0;  // can't read more

    if(in_buffer.read_pos == NULL)        // first read, so just fill buffer
    {
        to_be_read = INPUT_BUFFER_SIZE;
        to_be_saved = 0;
        buffer_write_pos = in_buffer.buffer;
        in_buffer.stream_offset = (off_t) 0;
    } else                                            //we have allready read something
    {
        to_be_read = in_buffer.read_pos - in_buffer.buffer;
        to_be_saved = (ssize_t) INPUT_BUFFER_SIZE - to_be_read;
        if (to_be_saved > INPUT_BUFFER_SIZE / 2) panic("buffer error: reading to half full buffer",NULL,NULL);
        memcpy(in_buffer.buffer,in_buffer.read_pos,to_be_saved);    // move "low water" part to beginning of buffer
        buffer_write_pos = in_buffer.buffer + to_be_saved;
        in_buffer.stream_offset += (off_t) to_be_read;
        if(in_buffer.block_end != NULL) in_buffer.block_end -= to_be_read;
    }

    in_buffer.read_pos = in_buffer.buffer;

    read_count = 0;
    do
    {
         last_read = read(in_stream->fd,buffer_write_pos + read_count,(size_t) (to_be_read - read_count));
         if (last_read == -1) panic("Error reading file",in_stream->file,strerror(errno));
         if (last_read == 0) 
         { 
             if (close(in_stream->fd) == -1) panic("Error in closing file",in_stream->file,strerror(errno));
             in_stream = in_stream->next;
             if (in_stream != NULL) 
                 in_stream->start_offset = in_buffer.stream_offset + (off_t) read_count + (off_t) to_be_saved;
         }
         read_count += last_read;
    } while (in_stream != NULL && read_count < to_be_read);

    if (read_count < to_be_read) in_buffer.stream_end = buffer_write_pos + read_count - 1;

    return read_count;
}

/* reads byte from the buffer */
inline unsigned char  
read_byte()
{
    return *in_buffer.read_pos;
}

/* returns pointer to the read position */
inline unsigned char *
read_pos()
{
    return in_buffer.read_pos;
}

/* return the block end pointer */
inline unsigned char *
block_end_pos()
{
    return in_buffer.block_end;
}

/* advances the read pointer, if buffer has reached low water, get more from stream to buffer */
/* returns false in case of end of stream */

inline int 
get_next_byte()
{
    if(in_buffer.read_pos >= in_buffer.low_pos) 
    {
        read_input_stream(); 
        if(in_buffer.block_end == NULL) mark_block_end();
    }

    if(in_buffer.stream_end != NULL)
    {
        if(in_buffer.read_pos >= in_buffer.stream_end)
        {
            return 0;
        }
    } 

    in_buffer.read_pos++;
    in_buffer.block_offset++;
    return 1;
}

/* check if the eof current block is in buffer and mark it in_buffer.block_end */
void
mark_block_end()
{
    unsigned char *safe_search,*scan;
    int i;

    if(in_buffer.stream_end != NULL)
    {
        safe_search = in_buffer.stream_end;
    } else
    {
        safe_search = in_buffer.buffer + INPUT_BUFFER_SIZE;
    }
    
    in_buffer.block_end = NULL;

    if(block.type & BLOCK_STOP_M)
    {
        in_buffer.block_end = in_buffer.read_pos + (block.stop.M - in_buffer.block_offset - 1);
        if(in_buffer.block_end > safe_search) in_buffer.block_end = NULL;
    }


    if(block.type & BLOCK_STOP_S)
    {
        scan = in_buffer.read_pos;
        if(block.stop.S.length)
        {
            if(block.type & BLOCK_START_S && in_buffer.block_offset < block.start.S.length) 
                scan += block.start.S.length - in_buffer.block_offset;
            i = 0;
            while(scan <= safe_search - block.stop.S.length + 1 && i < block.stop.S.length) 
            {
                i = 0;
                while(*scan == block.stop.S.string[i] && i < block.stop.S.length)
                {
                    scan++;
                    i++;
                }
                if(i) 
                {
                    scan -= i - 1;
                } else
                {
                    scan++;
                }
            } 

            if (i == block.stop.S.length)
            {
                scan += i - 2;
                in_buffer.block_end = scan;
            }
        } else
        {
            if(block.type & BLOCK_START_S)
            {
                if(block.start.S.length)
                {
                    if(in_buffer.block_offset < block.start.S.length)          // to skip block start
                        scan += block.start.S.length - in_buffer.block_offset;

                    i = 0;

                    while(scan <= safe_search - block.start.S.length + 1 && i < block.start.S.length) 
                    {
                        i = 0;
                        while(*scan == block.start.S.string[i] && i < block.start.S.length)
                        {
                            scan++;
                            i++;
                        }
                        if(i) 
                        {
                            scan -= i - 1;
                        } else
                        {
                            scan++;
                        }
                    }

                    if (i == block.start.S.length)
                    {
                        in_buffer.block_end = scan - 2;
                    }
                } else
                {
                    panic("Both block start and stop zero size",NULL,NULL);
                }
            }
        }
    }

    if(in_buffer.block_end ==  NULL && in_buffer.stream_end != NULL) 
        in_buffer.block_end = in_buffer.stream_end;
}

/* returns true if current byte is last in block */
inline int
last_byte()
{
    return in_buffer.block_end == in_buffer.read_pos;
}

/* returns true if end of stream has been reached */
inline int
end_of_stream()
{
    if(in_buffer.stream_end != NULL && in_buffer.stream_end == in_buffer.read_pos) 
    {
        return 1;
    } else
    {
        return 0;
    }
}

/* read for stream to input buffer and advance the read_pos to the start of the buffer */
/* in_buffer.read_pos should point to last byte of previous block */
int
find_block()
{
    unsigned char *safe_search,*scan_start;
    register int i;
    int found;

    found = 0;

    if(end_of_stream() && last_byte()) return 0;

    if(in_buffer.read_pos == NULL)  // first read
    {
        if(!read_input_stream()) return 0;  // zero size input
    }
    
    in_buffer.block_offset = 0;


    do
    {
        if(in_buffer.read_pos >= in_buffer.low_pos) read_input_stream();

        if(last_byte()) in_buffer.read_pos++;
        in_buffer.block_end = NULL;

        scan_start = in_buffer.read_pos;

        if(in_buffer.stream_end != NULL)
        {
            safe_search = in_buffer.stream_end;
        } else
        {
            safe_search = in_buffer.low_pos;
        }

        if (in_buffer.read_pos <= safe_search)
        {
            if(block.type & BLOCK_START_M)
            {
                if(block.start.N >= in_buffer.stream_offset + (off_t) (in_buffer.read_pos-in_buffer.buffer) && 
                   block.start.N <= in_buffer.stream_offset + (off_t) (safe_search-in_buffer.buffer))
                {
                    in_buffer.read_pos = in_buffer.buffer + (block.start.N-in_buffer.stream_offset);
                    found = 1;
                } else
                {
                    in_buffer.read_pos = safe_search;
                }
            }

            if(block.type & BLOCK_START_S)
            {
                if(block.start.S.length > 0)
                {
                    i = 0;
                    if(in_buffer.stream_end == NULL) safe_search += block.start.S.length - 1;
                    while(in_buffer.read_pos <= safe_search - block.start.S.length + 1 && i < block.start.S.length)
                    {
                        i = 0;
                        while(*in_buffer.read_pos == block.start.S.string[i] && i < block.start.S.length)
                        {
                            in_buffer.read_pos++;
                            i++;
                        }
                        if(i) 
                        {
                            in_buffer.read_pos -= i - 1;
                        } else
                        {
                            in_buffer.read_pos++;
                        }
                    }

                    if(i == block.start.S.length)
                    {
                        in_buffer.read_pos--;
                        found = 1;
                    } else if(scan_start == in_buffer.read_pos)
                    {
                        in_buffer.read_pos++;
                    }

                    if(in_buffer.read_pos > in_buffer.stream_end && in_buffer.stream_end !=  NULL) in_buffer.read_pos--;

                } else
                {
                    found = 1;
                }
            }
            if(in_buffer.read_pos > scan_start && !output_only_block) 
                write_output_stream(scan_start,in_buffer.read_pos - scan_start);
            if(found) mark_block_end();
        }
    } while (!found && !end_of_stream());
    if(end_of_stream() && !found && !output_only_block) write_output_stream(in_buffer.read_pos,1);
    if(found) in_buffer.block_num++;
    return found;
}

/* write null terminated string */
void
write_string(char *string)
{
    register char *f;

    f = string;

    while(*f != 0) f++;

    write_buffer(string,(off_t) (f - string));
}

/* write_buffer at the current write position */
void
write_buffer(unsigned char *buf,off_t length)
{

    if(!length) return;

    if(out_buffer.write_pos + length >= out_buffer.end)
    {
        if(out_buffer.write_pos == out_buffer.buffer) panic("Out buffer too small, should not happen!",NULL,NULL);
        flush_buffer();
    }
    memcpy(out_buffer.write_pos,buf,length);
    out_buffer.write_pos += length;
    out_buffer.block_offset += length;
}

/* put_byte, put one byte att current write position */
inline void
put_byte(unsigned char byte)
{
    *out_buffer.write_pos = byte;
}

/* next_byte, advance the write pointer by one */
/* if buffer full write it to disk */
inline void
write_next_byte()
{
    out_buffer.write_pos++;
    out_buffer.block_offset++;
    if(out_buffer.write_pos >= out_buffer.end)
    {
        flush_buffer();
    }
}

/* write unwritten data from buffer to disk */
void
flush_buffer()
{
    write_output_stream(out_buffer.buffer,out_buffer.write_pos - out_buffer.buffer);
    write_w_command(out_buffer.buffer,out_buffer.write_pos - out_buffer.buffer);
    out_buffer.write_pos = out_buffer.buffer;
}

/* close_output_stream */
void
close_output_stream()
{
    if(close(out_stream.fd) == -1) panic("Error closing output stream",out_stream.file,strerror(errno));
}

