/*
Copyright (C) 2013-2014 Roelof Nico du Toit.

@description Editor implementation

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

#ifndef __LIBCHARS_EDITOR_H__
#define __LIBCHARS_EDITOR_H__

#include "terminal.h"

#include <string>

namespace libchars {

    enum mode_e {
        MODE_STRING,    // single line; no up/dn/pgup/pgdn; move/jump disabled; tab/help/del disabled; bksp enabled; validation disabled
        MODE_PASSWORD,  // same restrictions as MODE_STRING; display disabled
        MODE_MULTILINE, // no up/dn/pgup/pgdn; move/jump disabled; tab/help/del disabled; bksp enabled; no validation; exit with EOF; enter->newline
        MODE_COMMAND,   // all keys enabled; validation enabled
    };

    enum key_e {
        KEY_LEFT, KEY_RIGHT,
        KEY_UP, KEY_DOWN,
        KEY_PGUP, KEY_PGDN,
        KEY_SOL, KEY_EOL, 
        KEY_EOF, KEY_CLEAR,
        KEY_DEL, KEY_BKSP,
        KEY_TAB, KEY_ENTER,
        KEY_WIPE, KEY_SWAP,
        KEY_HELP, KEY_QUIT,
        PRINTABLE_CHAR,
        PARTIAL_SEQ,
        IGNORE_SEQ,
        SEQ_TIMEOUT,
    };

    class edit_object
    {
    private:
        const static size_t MAX_LINE = 16384;
        std::string buffer;
    public:
        const mode_e mode;
        std::string prompt;
        size_t insert_idx; // relative to buffer; current position
        size_t cursor;
        size_t prompt_rendered;
    private:
        void reset() {
            rewind();
            insert_idx = length();
        }
    public:
        edit_object(mode_e m = MODE_STRING, const char *s = NULL) : mode(m) { if (s!=NULL) buffer.assign(s); reset(); }
        edit_object(mode_e m, std::string &s) : buffer(s),mode(m) { reset(); }
        virtual ~edit_object() {}

        inline const std::string &value() const { return buffer; }
        inline size_t length() const { return buffer.length(); }
        inline size_t idx(size_t idx_in) { return idx_in > length() ? length() : idx_in;}
        inline void rewind() { cursor = 0; prompt_rendered = 0; }
        inline void clear() { insert_idx = 0; wipe(); reset(); }

        virtual void emptied() {} // called when edit string becomes empty

        virtual void set(const char *line, size_t idx = std::string::npos)
        {
            if (line != NULL) {
                size_t L = length();
                buffer.assign(line, 0, MAX_LINE);

                if (idx <= buffer.length())
                    insert_idx = idx;
                else
                    insert_idx = buffer.length();

                if (L > 0 && length() == 0)
                    emptied();
            }
            else {
                insert_idx = 0;
                wipe();
            }
        }

        virtual void insert(const char c)
        {
            if (length() < MAX_LINE) {
                buffer.insert(insert_idx,1,c);
                ++insert_idx;
            }
        }
        virtual void wipe()
        {
            size_t L = length();
            if (insert_idx < length()) {
                buffer.erase(insert_idx);
            }
            if (L > 0 && length() == 0)
                emptied();
        }
        virtual void del()
        {
            size_t L = length();
            if (insert_idx < length()) {
                buffer.erase(insert_idx, 1);
            }
            if (L > 0 && length() == 0)
                emptied();
        }
        virtual void bksp()
        {
            size_t L = length();
            if (insert_idx > 0) {
                --insert_idx;
                buffer.erase(insert_idx, 1);
            }
            if (L > 0 && length() == 0)
                emptied();
        }
        virtual void swap()
        {
            if (insert_idx > 0 && insert_idx <= length() && length() > 1) {
                size_t swap_idx = insert_idx;
                if (swap_idx == length())
                    --swap_idx;
                char c = buffer.at(swap_idx);
                buffer.at(swap_idx) = buffer.at(swap_idx - 1);
                buffer.at(swap_idx - 1) = c;
                if (insert_idx < length())
                    right();
            }
        }
        virtual void left(size_t N = 1)
        {
            if (insert_idx > 0) {
                if (N > insert_idx) {
                    insert_idx = 0;
                }
                else {
                    insert_idx -= N;
                }
            }
        }
        virtual void right(size_t N = 1)
        {
            if (insert_idx < length()) {
                if ((insert_idx + N) > length()) {
                    insert_idx = length();;
                }
                else {
                    insert_idx += N;
                }
            }
        }

        virtual size_t render(size_t buf_idx, size_t limit, std::string &sequence)
        {
            //NOTE: return number of *displayed* characters, which might
            // be different from sequence.length() (e.g. if color used)
            if (limit > 0) 
                sequence = buffer.substr(buf_idx);
            else 
                sequence.clear();
            return sequence.length();
        }

        virtual size_t terminal_idx(size_t buf_idx)
        {
            // translate buffer index --> start of rendered sequence for that index;
            // the values would only differ if the render() function output
            // has more or fewer displayed characters than the buffer
            return idx(buf_idx);
        }

        virtual size_t terminal_cursor(size_t buf_idx)
        {
            // translate buffer index --> cursor position in rendered sequence;
            // the values would only differ if the render() function output
            // has more or fewer displayed characters than the buffer
            return idx(buf_idx);
        }

        virtual size_t buffer_idx(size_t term_idx)
        {
            // translate rendered sequence position --> buffer index;
            // the values would only differ if the render() function output
            // has more or fewer displayed characters than the buffer
            return idx(term_idx);
        }

        virtual bool key_valid(key_e key) const
        {
            // check validity at insert_idx; typical use is to not allow TAB
            // key in the middle of a token
            return true;
        }
    };

    class editor
    {
    private:
        //- - - -
        terminal_driver &driver;
        edit_object *obj;
        enum { IDLE, RENDER_DEFER, RENDER_NOW } state;
        key_e k;
        //- - - -
        const static size_t MAX_DECODE_SEQUENCE = 16;
        uint8_t seq[MAX_DECODE_SEQUENCE];
        size_t seq_N;

    public:
        editor(terminal_driver &d) : driver(d),obj(NULL),state(IDLE),seq_N(0) {}

    private:
        key_e decode_key(uint8_t c);

        void request_render();
        inline bool must_render() { return state == RENDER_NOW; }

        size_t render(size_t buf_idx, size_t length);
        int print();

    public:
        int edit(edit_object &obj_ref, size_t timeout_s = 0);
        int edit(std::string &str, size_t timeout_s = 0);

        inline bool interactive() const { return driver.interactive(); }
        inline bool control() const { return driver.control(); }

        inline void newline() { driver.newline(); }
        inline void clear_screen() { driver.clear_screen(); }

        inline key_e key() { return k; } // key that triggered return in edit()
    };

}

#endif // __LIBCHARS_EDITOR_H__
