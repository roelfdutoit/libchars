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

#include "commands.h"
#include "debug.h"

namespace libchars {

    token::token(type_t ttype_, id_t ID_, id_t vtype_, const char *name_) :
          ttype(ttype_),ID(ID_),
          status(0),vtype(vtype_),
          offset(0),length(0),
          next(NULL)
    {
        if (name_ != NULL)
            name.assign(name_);
    }

    token::~token()
    {
        delete next;
    }


    parameter::parameter(token::id_t ID, const char *name) :
        token(FLAG,ID,ID_NOT_SET,name) {}

    parameter::parameter(token::id_t ID, const char *name, validator::id_t vtype) :
        token(KEY,ID,vtype,name) { status |= MANDATORY; }

    parameter::parameter(token::id_t ID, validator::id_t vtype) :
        token(VALUE,ID,vtype) { status |= MANDATORY; }

    void parameter::set_help(const char *help_)
    {
        if (help_ != NULL)
            help.assign(help_);
    }

    void parameter::set_default(const char *value_)
    {
        if (value_ != NULL) {
            value.assign(value_);
            status |= DEFAULT_SET;
            set_optional();
        }
    }

    void parameter::set_hidden()
    {
        status |= HIDDEN;
    }

    void parameter::set_optional()
    {
        status &= ~(MANDATORY);
    }


    commands::status_t commands::sort()
    {
        if (cmd == NULL)
            return NO_COMMAND;

        size_t p_idx = 0;
        size_t n_assigned = 0;
        size_t n_available = 0;
        token *T = NULL;
        parameters_t par = cmd->par;

        // find FLAG parameters
        T = t_par;
        while (T != NULL) {
            ++n_available;
            if ((T->status & (token::IS_QUOTED | token::SORTED)) == 0) {
                for (p_idx = 0; p_idx < par.size(); ++p_idx) {
                    parameter &P = par[p_idx];
                    if (!(P.status & token::SORTED) && P.ttype == token::FLAG) {
                        if (P.name == T->value) {
                            // full match on flag name
                            T->status |= (token::SORTED | token::IN_STRING);
                            T->ttype = token::FLAG;
                            T->name = P.name;
                            T->ID = P.ID;
                            T->value.clear();
                            P.status |= token::SORTED;
                            ++n_assigned;
                            break;
                        }
                        else if (P.name.compare(0, T->value.length(), T->value) == 0) {
                            // partial match on flag name
                            T->status |= token::PARTIAL_ARG;
                        }
                    }
                }
            }
            T = T->next;
        }

        // find values for KEY parameters; error if corresponding value not available
        T = t_par;
        while (T != NULL) {
            if ((T->status & (token::IS_QUOTED | token::SORTED)) == 0) {
                for (p_idx = 0; p_idx < par.size(); ++p_idx) {
                    parameter &P = par[p_idx];
                    if (!(P.status & token::SORTED) && P.ttype == token::KEY) {
                        if (P.name == T->value) {
                            // full match on key name
                            T->status &= ~token::PARTIAL_ARG;
                            T->status |= (token::SORTED | token::IN_STRING);
                            T->ttype = token::KEY;
                            T->name = P.name;
                            T->ID = P.ID;
                            if (T->next == NULL) {
                                LC_LOG_VERBOSE("KEY(%s): missing value", P.name.c_str());
                                T->value.clear();
                                return MISSING_VALUE;
                            }
                            else if (T->next->status & token::SORTED) {
                                LC_LOG_VERBOSE("KEY(%s): missing value", P.name.c_str());
                                T->value.clear();
                                return MISSING_VALUE;
                            }
                            else {
                                T->value.clear();
                                T = T->next;
                                T->status |= (token::SORTED | token::IN_STRING | token::IS_VALUE);
                                T->ttype = token::KEY;
                                T->name = P.name;
                                T->ID = P.ID;
                                T->vtype = P.vtype;
                                P.status |= token::SORTED;
                                n_assigned += 2;
                                break;
                            }
                        }
                        else if (P.name.compare(0, T->value.length(), T->value) == 0) {
                            // partial match on key name
                            T->status |= token::PARTIAL_ARG;
                        }
                    }
                }
            }
            T = T->next;
        }

        // make sure all mandatory KEY parameters were specified
        size_t n_pm = 0, n_po = 0;
        for (p_idx = 0; p_idx < par.size(); ++p_idx) {
            parameter &P = par[p_idx];
            switch (P.ttype) {
            case token::KEY:
                if ((P.status & (token::SORTED | token::MANDATORY)) == token::MANDATORY) {
                    LC_LOG_VERBOSE("KEY(%s): missing key", P.name.c_str());
                    return TOO_FEW_ARGS;
                }
                break;
            case token::VALUE:
                if (P.status & token::MANDATORY) {
                    ++n_pm;
                }
                else {
                    ++n_po;
                }
                break;
            default:;
            }
        }

        // extract positional arguments (type = VALUE)
        size_t n_arguments = n_available - n_assigned;
        p_idx = 0;
        T = t_par;
        while (n_arguments > 0 && T != NULL) {
            while (T != NULL && (T->status & token::SORTED))
                T = T->next;
            if (T == NULL)
                break;
            while (p_idx < par.size() && ((par[p_idx].status & token::SORTED) || par[p_idx].ttype != token::VALUE))
                ++p_idx;
            if (p_idx >= par.size())
                break;
            parameter& P = par[p_idx];
            T->status |= (token::SORTED | token::IN_STRING | token::IS_VALUE);
            T->ttype = token::VALUE;
            T->vtype = P.vtype;
            T->ID = P.ID;
            P.status |= token::SORTED;
            --n_arguments;
            if (P.status & token::MANDATORY) --n_pm; else --n_po;
        }

        // mark remaining arguments as invalid (if not a partial key match)
        T = t_par;
        while (T != NULL) {
            while (T != NULL && (T->status & token::SORTED))
                T = T->next;
            if (T == NULL)
                break;
            T->status |= (token::SORTED | token::IN_STRING);
            if (!(T->status & token::PARTIAL_ARG))
                T->status |= token::INVALID;
            T->ttype = token::UNKNOWN;
        }

        if (n_arguments > 0) {
            LC_LOG_VERBOSE("Too many arguments [%zu extra]",n_arguments);
            return TOO_MANY_ARGS;
        }
        else if (n_pm > 0) {
            LC_LOG_VERBOSE("Missing mandatory arguments [%zu remain]",n_pm);
            return TOO_FEW_ARGS;
        }

        // add unspecified optional parameters (with default values) to token list
        token *t_head = (t_par != NULL) ? t_par : t_cmd;
        while (t_head != NULL && t_head->next != NULL)
            t_head = t_head->next;

        if (t_head != NULL) {
            for (p_idx = 0; p_idx < par.size(); ++p_idx) {
                parameter &P = par[p_idx];
                if ((P.status & (token::SORTED | token::DEFAULT_SET)) == token::DEFAULT_SET) {
                    switch (P.ttype) {
                    case token::FLAG:
                        // missing flag = FALSE
                        break;
                    case token::VALUE:
                        T = new token(P.ttype,P.ID,P.vtype);
                        T->status |= (token::SORTED | token::DEFAULT_USED);
                        T->value = P.value;
                        t_head->next = T;
                        t_head = T;
                        if (t_par == NULL)
                            t_par = T;
                        break;
                    case token::KEY:
                        T = new token(P.ttype,P.ID,P.vtype,P.name.c_str());
                        T->status |= (token::SORTED);
                        t_head->next = T;
                        t_head = T;
                        if (t_par == NULL)
                            t_par = T;
                        T = new token(P.ttype,P.ID,P.vtype,P.name.c_str());
                        T->status |= (token::SORTED | token::IS_VALUE | token::DEFAULT_USED);
                        T->value = P.value;
                        t_head->next = T;
                        t_head = T;
                        break;
                    default:;
                    }
                }
            }
        }

        return VALID_COMMAND;
    }

    void commands::show_parameters()
    {
        //TODO: use terminal driver to dump sequence
        //TODO: align and distribute better

        if (cmd == NULL) {
            printf("** INTERNAL ERROR: command unknown **\n");
        }
        else if (cmd->par.empty()) {
            printf("%s : %s\n", cmd->cmd_str.c_str(), cmd->help.c_str());
        }
        else {
            size_t parameters_printed = 0;
            parameters_t &par = cmd->par;
            size_t p_idx;
            // {key,value} pairs
            for (p_idx = 0; p_idx < par.size(); ++p_idx) {
                parameter &P = par[p_idx];
                if (P.ttype == token::KEY && !(P.status & token::HIDDEN)) {
                    bool mandatory = (P.status & token::MANDATORY);
                    printf("%c%s%c = <arg>",mandatory?'<':'[',P.name.c_str(),mandatory?'>':']');
                    if (!P.help.empty())
                        printf(" : %s", P.help.c_str());
                    if (!mandatory  && !P.value.empty())
                        printf(" (default:%s)", P.value.c_str());
                    printf("\n");
                    ++parameters_printed;
                }
            }
            // positional arguments
            size_t argidx = 1;
            for (p_idx = 0; p_idx < par.size(); ++p_idx) {
                parameter &P = par[p_idx];
                if (P.ttype == token::VALUE && !(P.status & token::HIDDEN)) {
                    bool mandatory = (P.status & token::MANDATORY);
                    printf("%carg%zu%c",mandatory?'<':'[',argidx++,mandatory?'>':']');
                    if (!P.help.empty())
                        printf(" : %s", P.help.c_str());
                    if (!mandatory  && !P.value.empty())
                        printf(" (default:%s)", P.value.c_str());
                    printf("\n");
                    ++parameters_printed;
                }
            }
            // flags
            bool type_seen = false;
            for (p_idx = 0; p_idx < par.size(); ++p_idx) {
                parameter &P = par[p_idx];
                if (P.ttype == token::FLAG && !(P.status & token::HIDDEN)) {
                    if (!type_seen) {
                        type_seen = true;
                        printf("====== optional flags ======\n");
                    }
                    printf("[%s]",P.name.c_str());
                    if (!P.help.empty())
                        printf(" : %s", P.help.c_str());
                    printf("\n");
                    ++parameters_printed;
                }
            }

            if (parameters_printed == 0)
                // ended up here because of hidden parameters
                printf("%s : %s\n", cmd->cmd_str.c_str(), cmd->help.c_str());
            else
                printf("\n");
        }
    }

}

