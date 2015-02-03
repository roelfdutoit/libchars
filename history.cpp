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

#include "history.h"

#include <list>
#include <string>

namespace libchars {

    void history::load(const std::string &line)
    {
        if (!line.empty() && !busy) {
            lines.push_back(line);
        }
    }

    void history::add(const std::string &line)
    {
        if (!line.empty()) {
            cancel();
            if (lines.empty() || lines.back() != line) {
                lines.push_back(line);
                persist();
            }
        }
    }

    void history::set(const std::string &line)
    {
        cancel();
        line_tmp = line;
    }

    bool history::prev()
    {
        if (!busy) {
            if (lines.empty()) {
                return false;
            }
            else {
                // use last value in list
                li = lines.end();
                --li;
                overflow = false;
                busy = true;
                return true;
            }
        }
        else {
            if (li == lines.begin()) {
                // stay on current value
                return false;
            }
            else {
                // use previous value
                --li;
                return true;
            }
        }
    }

    bool history::next()
    {
        if (!busy)
            return false;

        if (lines.empty() || overflow || li == lines.end()) {
            busy = false;
            return false;
        }

        if (++li == lines.end()) {
            busy = false;
            overflow = true;
            // revert back to temporary string
            return true;
        }
        else {
            // use next value
            return true;
        }
    }

    bool history::search(size_t idx)
    {
        if (lines.empty()) {
            busy = false;
            return false;
        }
        else {
            li = lines.end();
            overflow = true;
            s_idx = (idx > line_tmp.length()) ? line_tmp.length() : idx;
            busy = true;
            if (!search_prev() || overflow) {
                busy = false;
                return false;
            }
            return true;
        }
    }

    bool history::search_prev()
    {
        if (!busy)
            return false;

        history_lines_t::const_iterator li_current(li);

        while (li != lines.begin()) {
            --li;
            const std::string &lis = *li;
            if (s_idx == 0 || (lis.length() >= s_idx && line_tmp.compare(0,s_idx,lis,0,s_idx) == 0)) {
                overflow = false;
                return true;
            }
        }

        // stay on previous value
        li = li_current;
        return true;
    }

    bool history::search_next()
    {
        if (!busy)
            return false;

        if (lines.empty() || overflow || li == lines.end()) {
            busy = false;
            return false;
        }

        while (li != lines.end()) {
            if (++li != lines.end()) {
                const std::string& lis = *li;
                if (s_idx == 0 || (lis.length() >= s_idx && line_tmp.compare(0,s_idx,lis,0,s_idx) == 0))
                    return true;
            }
        }

        // revert back to temporary string
        busy = false;
        overflow = true;
        return true;
    }

    const char * history::current() const
    {
        if (overflow)
            return line_tmp.empty() ? NULL : line_tmp.c_str();
        else if (!busy)
            return NULL;
        else
            return li->c_str();
    }

    void history::cancel()
    {
        busy = false;
        overflow = false;
    }
}
