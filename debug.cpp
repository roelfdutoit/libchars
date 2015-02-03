/*
Copyright (C) 2013-2014 Roelof Nico du Toit. 
 
@description Debug log mechanism

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

#include "debug.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

namespace libchars {

    static FILE *_d_ = NULL;

    static const char *LC_LOG_DEFAULT_FILE = "debug.log";

    void debug::initialize__()
    {
        if (_d_ != NULL) {
            fclose(_d_);
            _d_ = NULL;
        }

        if (!log_path.empty())
            _d_ = fopen(log_path.c_str(), "at");
        else
            _d_ = fopen(LC_LOG_DEFAULT_FILE, "at");

        initialized = true;
    }

    debug::~debug()
    {
        if (_d_ != NULL) {
            fclose(_d_);
            _d_ = NULL;
        }
    }

    void debug::log(unsigned int lvl, const char *file, unsigned int line, const char *function, const char *format, ...)
    {
        if (lvl <= log_level) {
            if (!initialized) {
                initialize__();
            }
            if (_d_ != NULL) {
                static __thread char buffer[8192];
                va_list arguments;
                va_start(arguments, format);
                snprintf(buffer, sizeof(buffer), "%s:%u:%s() ", file, line, function);
                vsnprintf(buffer+strlen(buffer), sizeof(buffer)-strlen(buffer), format, arguments);
                va_end(arguments);
                fprintf(_d_,"%s\n",buffer);
                fflush(_d_);
            }
        }
    }

}

