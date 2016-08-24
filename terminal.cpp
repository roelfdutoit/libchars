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

#include "terminal.h"
#include "debug.h"

#include <ctype.h>
#include <errno.h>
#include <assert.h>
#include <unistd.h>

#include <sys/ioctl.h>
#include <sys/select.h>

#include <new>

namespace libchars {

    const static uint64_t LC_WINDOW_SIZE_UPDATE_TIMEOUT_ms = 400;
    const static uint64_t LC_CONTROL_CHECK_TIMEOUT_ms = 2000;
    const static uint64_t LC_CURSOR_POSITION_READ_TIMEOUT_ms = 5000;

    terminal_driver::terminal_driver() :
        T_must_return_ms(0),
        is_tty(true),
        fd_r(-1),fd_w(-1),
        changed(false),
        size_initialized(false),
        size_not_accurate(false),
        control_enabled(false),
        t_cols(0),t_rows(0),
        pos_x(0),pos_y(0),
        rbuf(NULL),
        rbuf_size_log2(0),rbuf_size(0),
        rbuf_enq(0),rbuf_deq(0)
    {
        T_ws_updated.tv_sec = 0;
        T_ws_updated.tv_usec = 0;
        reallocate(10);
    }

    terminal_driver::~terminal_driver()
    {
        shutdown();
    }

    int terminal_driver::initialize__(int fd_in, int fd_out)
    {
        LC_LOG_DEBUG("START:%d,%d",fd_in,fd_out);
        fd_r = fd_in;
        fd_w = fd_out;

        if (fd_r >= 0 && fd_w >= 0) {
            // read terminal settings
            is_tty = isatty(fd_r);
            const char *term_tty_name = is_tty ? ttyname(fd_r) : "-";
            LC_LOG_DEBUG("INIT:%s",term_tty_name);

            (void)tcgetattr(fd_r, &original_termios);

            // put terminal in raw mode
            struct termios new_termios;
            (void)tcgetattr(fd_r, &new_termios);
            new_termios.c_iflag = 0;
            new_termios.c_oflag = OPOST | ONLCR;
            new_termios.c_lflag = 0;
            new_termios.c_cc[VMIN] = 1;
            new_termios.c_cc[VTIME] = 0;
            (void)tcsetattr(fd_r, TCSADRAIN, &new_termios);

            if (is_tty) {
                // auto-disable control sequences if cursor position reply not seen before timeout
                size_t x,y;
                control_enabled = (cursor_position(x,y,LC_CONTROL_CHECK_TIMEOUT_ms) >= 0);
                if (!control_enabled) 
                    newline();
                get_terminal_width_and_height(); 
            }
        }
        return 0;
    }

    void terminal_driver::shutdown()
    {
        if (fd_r >= 0) {
            const char *term_tty_name = is_tty ? ttyname(fd_r) : "-";
            LC_LOG_DEBUG("RESTORE:%s",term_tty_name);
            // restore terminal settings
            (void)tcsetattr(fd_r, TCSADRAIN, &original_termios);
            fd_r = -1;
        }
        delete[] rbuf;
        rbuf = NULL;
    }

    int terminal_driver::reallocate(size_t size_log2)
    {
        //NOTE: only accomodate increase in size
        if (size_log2 < rbuf_size_log2)
            return -1;

        if (size_log2 > rbuf_size_log2) {
            uint8_t *rbuf_new = new (std::nothrow) uint8_t[1<<size_log2];
            if (rbuf_new == NULL)
                return -2;

            if (rbuf_enq > rbuf_deq) {
                // copy from old buffer to new buffer
                size_t rbuf_mask = (rbuf_size - 1);
                size_t idx_enq = (rbuf_enq & rbuf_mask);
                size_t idx_deq = (rbuf_deq & rbuf_mask);
                if (idx_enq < idx_deq) {
                    // copy from idx_deq --> end
                    memcpy(rbuf_new, rbuf + idx_deq, rbuf_size - idx_deq);
                    // copy from 0 --> idx_enq
                    memcpy(rbuf_new + rbuf_size - idx_deq, rbuf, idx_enq);
                }
                else {
                    // copy from idx_deq to idx_enq
                    memcpy(rbuf_new, rbuf + idx_deq, idx_enq - idx_deq);
                }
            }

            delete[] rbuf;
            rbuf = rbuf_new;
            rbuf_size_log2 = size_log2;
            rbuf_size = (1<<rbuf_size_log2);
        }

        return 0;
    }

    int terminal_driver::read_characters(bool skip_force_check)
    {
        while (true) {
            struct timeval T_now;
            if (gettimeofday(&T_now, NULL) == 0) {
                uint64_t t_msec_now = T_now.tv_sec * 1000ULL + T_now.tv_usec/1000ULL;
                uint64_t t_msec_then = T_ws_updated.tv_sec * 1000ULL + T_ws_updated.tv_usec/1000ULL;
                if (t_msec_now >= (t_msec_then + LC_WINDOW_SIZE_UPDATE_TIMEOUT_ms)) {
                    get_terminal_width_and_height();
                    T_ws_updated = T_now;
                }
                if (!skip_force_check && T_must_return_ms > 0 && t_msec_now > T_must_return_ms) {
                    T_must_return_ms = 0;
                    return -4;
                }
            }

            fd_set f_io;
            FD_ZERO(&f_io);
            FD_SET(fd_r, &f_io);
            struct timeval T_io = { 0, LC_WINDOW_SIZE_UPDATE_TIMEOUT_ms*1000ULL };
            int r = select(fd_r+1, &f_io, NULL, NULL, &T_io);
            if (r == 0) {
                return 0;
            }
            else if (r > 0 && FD_ISSET(fd_r, &f_io)) {
                size_t rbuf_slots_used = (rbuf_enq - rbuf_deq);
                if (rbuf_slots_used >= rbuf_size) {
                    if (rbuf_size_log2 >= RBUF_SIZE_LOG2_MAX)
                        return -1; //XXX: cannot go any bigger
                    if (reallocate(rbuf_size_log2 + 1) != 0)
                        return -2;
                }

                size_t rbuf_mask = (rbuf_size - 1);
                size_t rbuf_slots_avail = (rbuf_size - rbuf_slots_used);
                size_t idx_enq = (rbuf_enq & rbuf_mask);
                if ((idx_enq + rbuf_slots_avail) > rbuf_size)
                    rbuf_slots_avail = (rbuf_size - idx_enq);

                ssize_t n = ::read(fd_r, rbuf + idx_enq, rbuf_slots_avail);
                if (n > 0) {
                    rbuf_enq += n;
                    return 1;
                }
                else if (n < 0 && errno!=EINTR && errno!=EWOULDBLOCK) {
                    return -1;
                }
            }
            else if (r < 0 && errno!=EINTR && errno!=EWOULDBLOCK) {
                return -1;
            }
        }
    }

    void terminal_driver::get_terminal_width_and_height()
    {
        if (!control_enabled) {
            t_cols = t_rows = 0;
            changed = false;
        }
        else if (fd_w > 0) {
            if (!size_initialized) {
                struct winsize ws;
                ws.ws_col = 0;
                if (ioctl(fd_w, TIOCGWINSZ, &ws) >= 0) {
                    size_initialized = true;
                    t_cols = ws.ws_col;
                    t_rows = ws.ws_row;
                    // make sure size is accurate
                    cursor_save();
                    cursor_to_xy(t_cols + 1, t_rows + 1);
                    size_t x = 0, y = 0;
                    cursor_position(x,y);
                    cursor_restore();
                    if (x != t_cols || y != t_rows) {
                        t_cols = x;
                        t_rows = y;
                        size_not_accurate = true;
                    }
                }
            }
            else if (size_not_accurate) {
                // do not try to update window size
                return;
            }
            else {
                struct winsize ws;
                ws.ws_col = 0;
                if (ioctl(fd_w, TIOCGWINSZ, &ws) >= 0) {
                    if (t_cols != ws.ws_col || t_rows != ws.ws_row) {
                        LC_LOG_VERBOSE("window[%zux%zu]-->[%ux%u]",t_cols,t_rows,ws.ws_col,ws.ws_row);
                        changed = (t_cols != 0 || t_rows != 0);
                        t_cols = ws.ws_col;
                        t_rows = ws.ws_row;
                    }
                }
            }
        }
        else {
            t_cols = t_rows = 0;
            changed = true;
        }
    }

    int terminal_driver::cursor_position(size_t &x, size_t &y, ssize_t timeout_ms)
    {
        x = y = 0;

        //XXX: do not check for 'control_enabled' because this function is used to determine 'control_enabled'

        if (timeout_ms < 0)
            timeout_ms = LC_CURSOR_POSITION_READ_TIMEOUT_ms;

        int r = write("\x1b[6n", 4);
        if (r != 0)
            return r;

        struct timeval T_start = { 0, 0 };
        gettimeofday(&T_start, NULL);

        while (true) {
            // search for pattern: ^[yyy;xxxR  (minimum of 6 characters)
            if (rbuf_enq >= (rbuf_deq + 6)) {
                enum { WAIT_ESC0, WAIT_ESC1, WAIT_SEMI, WAIT_TERM } seq_state = WAIT_ESC0;
                size_t rbuf_mask = (rbuf_size - 1);
                size_t rbuf_search = rbuf_deq;
                size_t rbuf_found = rbuf_size;
                while (rbuf_search < rbuf_enq) {
                    uint8_t c = *(rbuf + (rbuf_search & rbuf_mask));
                    switch (seq_state) {
                    case WAIT_ESC0:
                        if (c == 0x1b) {
                            seq_state = WAIT_ESC1;
                            rbuf_found = rbuf_search;
                        }
                        break;
                    case WAIT_ESC1:
                        if (c == '[') {
                            seq_state = WAIT_SEMI;
                        }
                        else {
                            seq_state = WAIT_ESC0;
                            rbuf_found = rbuf_size;
                        }
                        break;
                    case WAIT_SEMI:
                        if (c == ';') {
                            seq_state = WAIT_TERM;
                        }
                        else if (isdigit(c)) {
                            y = y*10 + (c-'0');
                        }
                        else {
                            seq_state = WAIT_ESC0;
                            rbuf_found = rbuf_size;
                        }
                        break;
                    case WAIT_TERM:
                        if (c == 'R') {
                            // pattern found; remove bytes from buffer
                            ++rbuf_search;
                            size_t rbuf_copy_to = rbuf_found;
                            size_t rbuf_copy_from = rbuf_search;
                            while (rbuf_copy_from < rbuf_enq) {
                                *(rbuf + (rbuf_copy_to & rbuf_mask)) = *(rbuf + (rbuf_copy_from & rbuf_mask));
                                ++rbuf_copy_to;
                                ++rbuf_copy_from;
                            }
                            rbuf_enq -= (rbuf_search - rbuf_found);
                            return 0;
                        }
                        else if (isdigit(c)) {
                            x = x*10 + (c-'0');
                        }
                        else {
                            seq_state = WAIT_ESC0;
                            rbuf_found = rbuf_size;
                        }
                        break;
                    }
                    ++rbuf_search;
                }
            }
            // pattern not found; need more characters
            r = read_characters(true);
            if (r < 0)
                return r;
            else if (r == 0) {
                // timeout logic
                struct timeval T_now;
                if (gettimeofday(&T_now, NULL) == 0) {
                    uint64_t t_msec_now = T_now.tv_sec * 1000ULL + T_now.tv_usec/1000ULL;
                    uint64_t t_msec_start = T_start.tv_sec * 1000ULL + T_start.tv_usec/1000ULL;
                    if (t_msec_now >= (t_msec_start + timeout_ms))
                        return -1;
                }
                else {
                    return -1;
                }
            }
        }
    }

    int terminal_driver::cursor_left(size_t N)
    {
        if (!control_enabled)
            return 0;
        char moveseq[32];
        sprintf(moveseq,"\x1b[%zuD",N);
        return write(moveseq,strlen(moveseq));
    }

    int terminal_driver::cursor_right(size_t N)
    {
        if (!control_enabled)
            return 0;
        char moveseq[32];
        sprintf(moveseq,"\x1b[%zuC",N);
        return write(moveseq,strlen(moveseq));
    }

    int terminal_driver::cursor_to_xy(size_t x, size_t y)
    {
        if (!control_enabled)
            return 0;
        char moveseq[32];
        if (x <= 1 && y <= 1) {
            sprintf(moveseq, "\x1b[;H"); // move to upper-left corner
        }
        else {
            sprintf(moveseq,"\x1b[%zu;%zuH",y,x); // set (x,y)
        }
        return write(moveseq,strlen(moveseq));
    }

    void terminal_driver::cursor_disable()
    {
        if (control_enabled) {
            const static char _seq_[] = { 27, '[', '?', '2', '5', 'l' };
            write(_seq_,6);
        }
    }

    void terminal_driver::cursor_enable()
    {
        if (control_enabled) {
            const static char _seq_[] = { 27, '[', '?', '2', '5', 'h' }; 
            write(_seq_,6);
        }
    }

    void terminal_driver::cursor_save()
    {
        if (control_enabled) {
            const static char _seq_[] = { 27, '7' };
            write(_seq_,2);
        }
    }

    void terminal_driver::cursor_restore()
    {
        if (control_enabled) {
            const static char _seq_[] = { 27, '8' };
            write(_seq_,2);
        }
    }

    void terminal_driver::clear_screen()
    {
        if (control_enabled) {
            write("\x1b[2J",4);
            cursor_to_xy(1,1);
        }
    }

    void terminal_driver::clear_to_end_of_screen()
    {
        if (control_enabled)
            write("\x1b[0J",4);
    }

    void terminal_driver::newline()
    {
        write("\n\r",2);
    }

    int terminal_driver::set_new_xy(ssize_t N)
    {
        if (!control_enabled)
            return 0;

        int r = 0;

        if (N != 0) {
            // (x0,y0) = current cursor position
            size_t x0,y0;
            cursor_position(x0,y0);

            // sanity check on (x0,y0) -- some terminal report wrong size
            if (y0 > rows()) {
                clear_screen();
                x0 = y0 = 1;
            }

            // calculate new cursor position (x1,y1)
            size_t x1,y1;
            if (t_cols == 0 || t_rows == 0) {
                x1 = x0; y1 = y0;
            }
            else {
                size_t idx_x0y0 = (y0-1) * t_cols + (x0-1);
                if (((ssize_t)idx_x0y0 + N) <= 0) {
                    x1 = 1; y1 = 1;
                    LC_LOG_VERBOSE("x0[%zu];y0[%zu];idx0[%zu] --> TOP-LEFT x1[%zu];y1[%zu]",x0,y0,idx_x0y0,x1,y1);
                }
                else if (((ssize_t)idx_x0y0 + N) >= (ssize_t)(t_cols*t_rows)) {
                    x1 = t_cols; y1 = t_rows;
                    LC_LOG_VERBOSE("x0[%zu];y0[%zu];idx0[%zu] --> BOTTOM-RIGHT x1[%zu];y1[%zu]",x0,y0,idx_x0y0,x1,y1);
                }
                else {
                    size_t idx_x1y1 = idx_x0y0 + N;
                    x1 = (idx_x1y1 % t_cols) + 1;
                    y1 = (idx_x1y1 / t_cols) + 1;
                    LC_LOG_VERBOSE("x0[%zu];y0[%zu];idx0[%zu] --> x1[%zu];y1[%zu];idx1[%zu]",x0,y0,idx_x0y0,x1,y1,idx_x1y1);
                }
            }

            // move to new position
            if (x1 == 1 && y1 == 1) {
                r = cursor_to_xy(x1,y1);
            }
            else if (y0 == y1) {
                if (x1 < x0)
                    r = cursor_left(x0 - x1);
                else if (x1 > x0)
                    r = cursor_right(x1 - x0);
            }
            else {
                r = cursor_to_xy(x1,y1);
            }
        }

        return r;
    }

    bool terminal_driver::size_changed()
    {
        bool changed_ = changed;
        changed = false;
        return changed_;
    }

    int terminal_driver::write(const char *sequence, size_t seqlen)
    {
        const char *const seqend = sequence + seqlen;

        while (sequence < seqend) {
            ssize_t n = ::write(fd_w, sequence, (size_t)(seqend - sequence));
            if (n > 0) {
                sequence += n;
            }
            else {
                if (n == 0)
                    return -1;
                if (errno != EINTR && errno != EWOULDBLOCK)
                    return -1;
            }
        }
        return 0;
    }

    int terminal_driver::read(uint8_t &c, size_t timeout_s)
    {
        if (rbuf_enq <= rbuf_deq) {
            struct timeval T_start;
            gettimeofday(&T_start, NULL);
            uint64_t t_msec_start = T_start.tv_sec * 1000ULL + T_start.tv_usec/1000ULL;
            uint64_t timeout_ms = timeout_s * 1000UL;

            while (rbuf_enq <= rbuf_deq) {
                int r = read_characters(false);
                if (r < 0)
                    return r;
                else if (r == 0) {
                    if (timeout_s > 0) {
                        struct timeval T_now;
                        if (gettimeofday(&T_now, NULL) == 0) {
                            uint64_t t_msec_now = T_now.tv_sec * 1000ULL + T_now.tv_usec/1000ULL;
                            if (t_msec_now >= (t_msec_start + timeout_ms))
                                return -3;
                        }
                        else {
                            return r;
                        }
                    }
                    else {
                        return r;
                    }
                }
            }
        }

        size_t rbuf_mask = (rbuf_size - 1);
        c = *(rbuf + (rbuf_deq & rbuf_mask));
        ++rbuf_deq;

        return 1;
    }

    bool terminal_driver::read_available() const
    {
        return rbuf_enq > rbuf_deq;
    }
    
    void terminal_driver::set_return_timeout(size_t timeout_s)
    {
        struct timeval T_now;
        if (timeout_s > 0 && gettimeofday(&T_now, NULL) == 0) {
            uint64_t t_msec_now = T_now.tv_sec * 1000ULL + T_now.tv_usec/1000ULL;
            this->T_must_return_ms = t_msec_now + timeout_s * 1000ULL;
        }
        else {
            this->T_must_return_ms = 0;
        }
    }

    void terminal_driver::clear_return_timeout()
    {
        this->T_must_return_ms = 0;
    }
}

