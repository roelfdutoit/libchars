/*
Copyright (C) 2013-2014 Roelof Nico du Toit.

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

#include "editor.h"

const char *COLOR_KEYWORD = "\x1b[1;34m";
const char *COLOR_NORMAL = "\x1b[0m";

struct cmd_obj : public libchars::edit_object
{
    cmd_obj(const char *str) : edit_object(libchars::MODE_COMMAND,str) {}

    const std::string value() const
    {
        std::string str;
        if (length() > 0 && data() != NULL)
            str.assign(data(),length());
        return str;
    }

    virtual size_t render(size_t buf_idx, size_t limit, std::string &sequence)
    {
        if ((buf_idx + limit) > length())
            limit = length() - buf_idx;

        if (buf_idx >= 10) {
            sequence.assign(value(), buf_idx, limit);
        }
        else {
            sequence.assign(COLOR_KEYWORD);
            if ((buf_idx + limit) <= 10) {
                sequence.append(value(), buf_idx, limit);
                sequence.append(COLOR_NORMAL);
            }
            else {
                sequence.append(value(), buf_idx, 10 - buf_idx);
                sequence.append(COLOR_NORMAL);
                sequence.append(value(), 10, limit - (10 - buf_idx));
            }
        }

        return limit;
    }
};

int main(void)
{
    libchars::terminal_driver &tdriver = libchars::terminal_driver::initialize();
    libchars::editor editor(tdriver);

    int tabs = 0;
    cmd_obj O_cmd("hello");
    O_cmd.prompt = "PROMPT:";

    while (true) {
        (void)editor.edit(O_cmd);
        switch (editor.key()) {
        case libchars::KEY_ENTER:
            fprintf(stdout,"\n%s\n",O_cmd.value().c_str());
            return 0;
        case libchars::KEY_TAB:
            if (++tabs > 2) {
                fprintf(stdout,"\n[COMPLETE]\n");
                return 0;
            }
            else {
                O_cmd.insert('X');
            }
            break;
        case libchars::KEY_HELP:
            //ignore
            break;
        default:
            fprintf(stdout,"\n[TERMINATE]\n");
            return 0;
        }
    }

    return 0;
}

