/*
Copyright (C) 2013-2014 Roelof Nico du Toit.

@description Command history

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

#ifndef __LIBCHARS_HISTORY_H__
#define __LIBCHARS_HISTORY_H__

#include <string>
#include <list>

namespace libchars {

    class history
    {
    public:
        history() : busy(false),overflow(false) {}
        virtual ~history() {}

    protected:
        bool busy; // search in progress
        std::string line_tmp; // current temporary line

        typedef std::list<std::string> history_lines_t;
        history_lines_t lines;
        history_lines_t::const_iterator li;
        bool overflow;
        size_t s_idx; // search index

    public:
        virtual void persist() {} // default = memory only

        void load(const std::string &line); // used initially to get history into memory

        void add(const std::string &line); // add validated command-line to persistent storage + cancel current search
        void set(const std::string &line); // set current temporary command-line (not persistent) + cancel current search

        inline bool searching() const { return busy; } // search is in progress

        bool prev(); // previous entry in history
        bool next(); // next entry in history

        bool search(size_t idx = 0); // find first match in history
        bool search_prev(); // search backward
        bool search_next(); // search forward

        inline size_t search_idx() const { return s_idx; }

        const char *current() const; // order: last set() -> last add() -> last find_X() result

        void cancel(); // cancel current search (e.g. new characters added)
    };

}

#endif // __LIBCHARS_HISTORY_H__
