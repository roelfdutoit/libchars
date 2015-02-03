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

#include "validation.h"
#include "commands.h"

namespace libchars {

    int validation::initialize__()
    {
        static bool initialized = false;
        if (!initialized) {
            //TODO: load default validators
            initialized = true;
        }
        return 0;
    }

    int validation::add_validator__(validator::id_t id, const validator *v)
    {
        if (v == NULL)
            return -1;
        if (id == validator::NONE)
            return -1;

        validators_by_id_t::const_iterator ii = i2v.find(id);
        if (ii != i2v.end())
            return -1; // duplicate

        if (!v->name.empty()) {
            vtype_by_name_t::const_iterator ni = n2i.find(v->name);
            if (ni != n2i.end())
                return -1; // duplicate
            n2i[v->name] = id;
        }

        i2v[id] = v;

        return 0;
    }

    validator::id_t validation::add_validator(const validator *v)
    {
        if (generator >= validator::USER)
            return -1; // no more space for auto-generated IDs
        else
            return add_validator__(generator++, v);
    }

    int validation::add_validator(validator::id_t id, const validator *v)
    {
        // check user-defined range
        if (id < validator::USER)
            return -1;
        else
            return add_validator__(id,v);
    }

    const validator::id_t validation::get_vtype_by_name(const char *name)
    {
        if (name == NULL)
            return validator::NONE;

        vtype_by_name_t::const_iterator ni = n2i.find(name);
        if (ni != n2i.end())
            return ni->second;
        else
            return validator::NONE;
    }

    const validator *validation::get_validator_by_id(validator::id_t id)
    {
        if (id == validator::NONE)
            return NULL;

        validators_by_id_t::const_iterator ii = i2v.find(id);
        if (ii != i2v.end())
            return ii->second;
        else
            return NULL;
    }

    void commands::validate()
    {
        // validate known values (type = KEY/VALUE) in token list
        token *T = t_par;
        while (T != NULL) {
            if (!(T->status & token::INVALID)) {
                switch (T->ttype) {
                case token::KEY:
                case token::VALUE:
                    {
                        const validator *v = validation::initialize().get_validator_by_id(T->vtype);
                        //TODO: remove quotes from strings
                        T->status &= ~token::PARTIAL_ARG;
                        if (v == NULL) {
                            T->status |= token::VALIDATED;
                        }
                        else {
                            switch (v->check(T->value)) {
                            case validator::INVALID:
                                break;
                            case validator::PARTIAL:
                                T->status |= token::PARTIAL_ARG;
                                break;
                            case validator::VALID:
                                T->status |= token::VALIDATED;
                                break;
                            }
                        }
                    }
                    break;
                case token::UNKNOWN:
                    // ignore
                    break;
                default:
                    T->status |= token::VALIDATED;
                }
            }
            T = T->next;
        }
    }

}

