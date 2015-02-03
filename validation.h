/*
Copyright (C) 2013-2014 Roelof Nico du Toit.

@description Parameter validation

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

#ifndef __LIBCHARS_VALIDATION_H__
#define __LIBCHARS_VALIDATION_H__

#include <string>
#include <map>

namespace libchars {

    struct validator
    {
        typedef int id_t;
        const static id_t NONE = 0;
        const static id_t INTERNAL = 1; // internal
        const static id_t AUTO = 128;   // auto-assigned
        const static id_t USER = 1024;  // user-defined

        typedef std::string name_t; // optional; used for lookup

        id_t id;
        name_t name;

        virtual ~validator() {}

        typedef enum { INVALID, PARTIAL, VALID } status_t;

        virtual status_t check(const std::string &value) const = 0;
    };

    class validation
    {
    private:
        validation() : generator(validator::AUTO) {}
        ~validation() {}

        validation(validation const&);
        void operator=(validation const&);

        int initialize__();

    private:
        typedef std::map<validator::name_t,validator::id_t> vtype_by_name_t;
        typedef std::map<validator::id_t,const validator*> validators_by_id_t;

        vtype_by_name_t n2i;
        validators_by_id_t i2v;
        validator::id_t generator;

    public:
        static validation& initialize()
        {
            static validation instance;
            instance.initialize__();
            return instance;
        }

    private:
        int add_validator__(validator::id_t id, const validator *v);

    public:
        validator::id_t add_validator(const validator *v); // auto-assign; caller owns pointer
        int add_validator(validator::id_t id, const validator *v); // internal / user-defined; caller owns pointer

        const validator::id_t get_vtype_by_name(const char *name);
        const validator *get_validator_by_id(validator::id_t id);
    };

}

#endif // __LIBCHARS_VALIDATION_H__
