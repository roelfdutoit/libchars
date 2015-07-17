/*
Copyright (C) 2013-2015 Roelof Nico du Toit.

@description VT100 terminal driver

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE. 
*/

#ifndef __LIBCHARS_TERMINAL_H__
#define __LIBCHARS_TERMINAL_H__

#include <stdint.h>
#include <termios.h>
#include <stdio.h>
#include <string.h>

#include <sys/time.h>

namespace libchars {

    class terminal_driver
    {
    private:
        struct termios original_termios;
        struct timeval T_ws_updated;
        bool is_tty;
        int fd_r; int fd_w;
        bool changed;
        bool size_initialized;
        bool size_not_accurate;
        bool control_enabled;
        size_t t_cols; size_t t_rows;
        size_t pos_x; size_t pos_y;

        uint8_t *rbuf;
        const static size_t RBUF_SIZE_LOG2_MAX = 20;
        size_t rbuf_size_log2;
        size_t rbuf_size;
        size_t rbuf_enq;
        size_t rbuf_deq;

    private:
        terminal_driver();
        ~terminal_driver();

        terminal_driver(terminal_driver const&);
        void operator=(terminal_driver const&);

        int initialize__(int fd_in, int fd_out);

    public:
        static terminal_driver& initialize(int fd_in = fileno(stdin), int fd_out = fileno(stdout))
        {
            static terminal_driver instance;
            if (instance.fd_r < 0 || instance.fd_w < 0) {
                instance.initialize__(fd_in, fd_out);
            }
            return instance;
        }

        void shutdown();

    private:
        int reallocate(size_t size_log2);
        int read_characters(size_t timeout_s = 0);
        void get_terminal_width_and_height();

    public:
        inline bool control() const { return control_enabled; }

        int cursor_left(size_t N);
        int cursor_right(size_t N);
        int cursor_to_xy(size_t x, size_t y);

        void cursor_disable();
        void cursor_enable();

        void cursor_save();
        void cursor_restore();

        inline size_t columns() const { return t_cols; }
        inline size_t rows() const { return t_rows; }

        inline bool interactive() const { return is_tty; }

        int cursor_position(size_t &x, size_t &y, ssize_t timeout_ms = -1);

        void clear_screen();
        void clear_to_end_of_screen();
        void newline();

        int set_new_xy(ssize_t N);

        bool size_changed();

        int write(const char *sequence, size_t seqlen);

        int read(uint8_t &c, size_t timeout_s = 0);

        bool read_available() const;

    public:
        struct auto_cursor {
            terminal_driver &driver;
            auto_cursor(terminal_driver &d) : driver(d) { driver.cursor_disable(); }
            ~auto_cursor() { driver.cursor_enable(); }
        };
    };

}

#endif // __LIBCHARS_TERMINAL_H__
