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

#ifndef __LIBCHARS_DEBUG_H__
#define __LIBCHARS_DEBUG_H__

#include <string>

namespace libchars {

    class debug
    {
    public:
        enum log_level_e {
            DISABLED = 0,
            ERROR,
            INFORMATION,
            DEBUG,
            VERBOSE,
        };

    private:
        unsigned int log_level;
        std::string log_path;
        bool initialized;

    private:
        debug(unsigned int lvl) : log_level(lvl),initialized(false) {}
        ~debug();

        debug(debug const&);
        void operator=(debug const&);

        void initialize__();

    public:
        static debug& initialize(unsigned int default_lvl = DISABLED)
        {
            static debug instance(default_lvl);
            return instance;
        }

        inline void set_level(unsigned int lvl) { log_level = lvl; }

        inline void set_path(const char *path) { if (path != NULL) { log_path.assign(path); initialized = false; } }

        inline bool check_level(unsigned int lvl) { return lvl <= log_level; }

        void log(unsigned int lvl, const char *file, unsigned int line, const char *function, const char *format, ...);
    };

}

#define LC_LOG_SET_LEVEL(level) do { libchars::debug::initialize().set_level(level); } while (0)
#define LC_LOG_SET_PATH(path) do { libchars::debug::initialize().set_path(path); } while (0)

#define LC_LOG_CHECK_LEVEL(level) (libchars::debug::initialize().check_level(level))

#define LC_LOG_x(level,...) do { libchars::debug::initialize().log((level),__FILE__,__LINE__,__FUNCTION__,__VA_ARGS__); } while (0)

#define LC_LOG_ERROR(...)   LC_LOG_x(libchars::debug::ERROR,__VA_ARGS__)
#define LC_LOG_INFO(...)    LC_LOG_x(libchars::debug::INFORMATION,__VA_ARGS__)
#define LC_LOG_DEBUG(...)   LC_LOG_x(libchars::debug::DEBUG,__VA_ARGS__)
#define LC_LOG_VERBOSE(...) LC_LOG_x(libchars::debug::VERBOSE,__VA_ARGS__)

#endif // __LIBCHARS_DEBUG_H__
