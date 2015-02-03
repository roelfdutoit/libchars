/*
Copyright (C) 2013-2014 Roelof Nico du Toit.

@description Command tokens and parameters

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

#ifndef __LIBCHARS_PARAMETER_H__
#define __LIBCHARS_PARAMETER_H__

#include "validation.h"

#include <string>
#include <vector>

namespace libchars {

    struct token
    {
        typedef int id_t;
        const static id_t ID_NOT_SET = -1;

        typedef std::string name_t;

        typedef enum { UNKNOWN, COMMAND, KEY, VALUE, FLAG } type_t;

        enum status_bits {
            VALIDATED    = 0x00000001,  // validated based on type (only applies to type=VALUE)
            INVALID      = 0x00000002,  // invalid (even before validation)
            PARTIAL_ARG  = 0x00000004,  // token matches first part of a valid KEY/FLAG
            IN_STRING    = 0x00000010,  // available in input string (vs using default value)
            SORTED       = 0x00000100,  // used internally by parameter sorting logic
            IS_QUOTED    = 0x00000400,  // original token in input string had quotes
            IS_VALUE     = 0x00000800,  // token is the value for type=KEY/VALUE
            MANDATORY    = 0x00001000,  // parameter is mandatory
            HIDDEN       = 0x00002000,  // do not display as part of context sensitive help
            DEFAULT_USED = 0x00004000,  // default value of parameter used
            DEFAULT_SET  = 0x00008000,  // default value of parameter available
        };

        //- - - - - - - - - - - - - - - - - - -

        std::string value; // token value; does not apply if type=FLAG

        name_t name; // optional; no whitespace; only applies if type=KEY/FLAG
        type_t ttype; // token type
        std::string help; // optional; whitespace allowed
        id_t ID; // optional; different "ID spaces" for commands and parameters; value has same ID as key
        uint32_t status; // combination of TOK_xxx values

        validator::id_t vtype; // optional; value type ID (ipv4,etc,including user-defined types); used by validator

        size_t offset; // index into command string (if applicable)
        size_t length; // length of token in command string (if applicable)

        struct token *next; // next element in linked-list
        //- - - - - - - - - - - - - - - - - - -

        token(type_t ttype = UNKNOWN, id_t ID = ID_NOT_SET, validator::id_t vtype = validator::NONE, const char *name = NULL);
        ~token();
    };

    class parameter : public token
    {
    public:
        parameter(token::id_t ID, const char *name); // type=FLAG
        parameter(token::id_t ID, const char *name, validator::id_t vtype); // type=KEY
        parameter(token::id_t ID, validator::id_t vtype = validator::NONE); // type=VALUE (positional)

    public:
        void set_help(const char *help); // context-sensitive help on parameter
        void set_default(const char *value); // also makes parameter optional
        void set_hidden(); // not displayed in help; ideally only optional parameters should be hidden
        void set_optional(); // use for case where parameter is optional + no default available
        //NOTE: optional positional arguments (type=VALUE) must note precede mandatory positional arguments
    };

    typedef std::vector<parameter> parameters_t;

}

#endif // __LIBCHARS_PARAMETER_H__
