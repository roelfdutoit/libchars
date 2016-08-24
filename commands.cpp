/*
Copyright (C) 2013-2015 Roelof Nico du Toit.

@description Commands engine

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

#include <new>
#include <memory>
#include <list>
#include <algorithm>

#include <assert.h>

namespace libchars {

    const char *commands::color_str(command_colors_e color_idx) const
    {
        if (!edit.control())
            return "";

        switch (color_idx) {
        case COLOR_NORMAL:           return "\x1b[0m";
        case COLOR_UNKNOWN_TOKEN:    return "\x1b[0;31m";
        case COLOR_VALID_COMMAND:    return "\x1b[0;32m";
        case COLOR_INVALID_COMMAND:  return "\x1b[1;31m";
        case COLOR_PARTIAL_COMMAND:  return "\x1b[0;33m";
        case COLOR_COMPLETION:       return "\x1b[0;36m";
        case COLOR_QUOTED_STRING:    return "\x1b[1;34m";
        case COLOR_VALID_ARGUMENT:   return "\x1b[1m";
        case COLOR_PARTIAL_ARGUMENT: return "\x1b[0m";
        case COLOR_INVALID_ARGUMENT: return "\x1b[1;31m";
        default: return "";
        }
    }

    command_node::command_node(const std::string &part_) :
        part(part_),mask(0),hidden(false),cmd(NULL),
        head(NULL),tail(NULL),
        next(NULL),start(NULL) {}

    command_node::command_node() :
        mask(0),hidden(false),cmd(NULL),
        head(NULL),tail(NULL),
        next(NULL),start(NULL) {}

    command_node::~command_node()
    {
        delete head;
        delete start;
        delete next;
    }

    void command_node::add_node(command_node *parent, command_node *n)
    {
        LC_LOG_VERBOSE("parent[%p/%s] + n[%p/%s]",parent,parent->part.c_str(),n,n->part.c_str());
        if (parent != NULL) {
            if (parent->head == NULL) {
                parent->head = parent->tail = n;
            }
            else {
                parent->tail->next = n;
                parent->tail = n;
            }
        }
    }

    command_node *command_node::add_node(command_node *parent, const std::string &part, command::filter_t mask_, bool hidden_)
    {
        if (parent != NULL) {
            command_node *n = new (std::nothrow) command_node(part);
            if (n != NULL) {
                n->mask = mask_;
                n->hidden = hidden_;
                add_node(parent, n);
            }
            return n;
        }
        else {
            return NULL;
        }
    }

    void command_node::clear()
    {
        delete head;
        delete start;
        delete next;

        mask = 0;
        hidden = false;
        cmd = NULL;
        head = NULL;
        tail = NULL;
        next = NULL;
        start = NULL;
    }

    command_node *command_node::add(const std::string &word, command::filter_t mask_, bool hidden_)
    {
        LC_LOG_VERBOSE("+word[%s] mask[0x%08x] hidden[%s]",word.c_str(),mask_,hidden_?"true":"false");

        mask |= mask_; // always update root mask

        if (head == NULL) {
            // no need to search; just add whole word
            LC_LOG_VERBOSE("add-new-root");
            return add_node(this,word,mask_,hidden_);
        }
        else {
            size_t ti = 0; // tree index (cumulative)
            size_t wi = 0; // index of first character in partial word (wi <= ti)
            size_t si = 0; // index into search word (0..length)

            command_node *root = this;
            command_node *prev = NULL;
            command_node *n = head;

            while (n != NULL && si < word.length()) {
                LC_LOG_VERBOSE("root[%p] prev[%p] n[%p] ti[%zu] wi[%zu] si[%zu]",root,prev,n,ti,wi,si);
                char c = word.at(si);
                size_t ri = (ti - wi); // index relative to partial dictionary word
                if (ri == 0) {
                    // check for match in first letter, else go to next command_node
                    if (n->part.at(ri) == c) {
                        ++si;
                        ++ti;
                    }
                    else if (n->next == NULL) {
                        return add_node(root,word.substr(si),mask_,hidden_);
                    }
                    else {
                        prev = n;
                        n = n->next;
                    }
                }
                else if (ri < n->part.length()) {
                    // check for match in current part
                    if (n->part.at(ri) == c) {
                        ++si;
                        ++ti;
                    }
                    else {
                        // mismatch = split + add new part
                        LC_LOG_VERBOSE("mismatch->split");
                        std::string part = n->part;
                        command_node *nn = new command_node;
                        nn->part.assign(part,0,ri);
                        nn->mask = n->mask | mask_;
                        nn->next = n->next;
                        n->part.assign(part.substr(ri));
                        n->next = NULL;
                        // split = move existing command_node down one level
                        add_node(nn,n);
                        if (prev == NULL) {
                            root->head = nn;
                        }
                        else {
                            prev->next = nn;
                        }
                        if (nn->next == NULL) {
                            root->tail = nn;
                        }
                        return add_node(nn,word.substr(si),mask_,hidden_);
                    }
                }
                else {
                    // reached end of current part; go down one level if possible
                    if (n->head == NULL) {
                        n->mask |= mask_;
                        return add_node(n,word.substr(si),mask_,hidden_);
                    }
                    else {
                        wi = ti;
                        root = n;
                        root->mask |= mask_;
                        prev = NULL;
                        n = n->head;
                    }
                }
            }

            LC_LOG_VERBOSE("END1: root[%p] prev[%p] n[%p] ti[%zu] si[%zu] word.length[%zu]",root,prev,n,ti,si,word.length());

            if (n == NULL) 
                n = root;

            LC_LOG_VERBOSE("END2: n[%p] wi[%zu] part.length[%zu]",n,wi,n->part.length());

            if (si < word.length()) {
                // not found --> add rest of word
                n->mask |= mask_;
                return add_node(n,word.substr(si),mask_,hidden_);
            }
            else if (si < (wi + n->part.length())) {
                // found, but only partially --> split + add new part
                LC_LOG_VERBOSE("found(partially)->split");
                size_t ri = (si - wi); // index relative to partial dictionary word
                std::string part = n->part;
                n->part.assign(part,0,ri);
                command_node *head__ = n->head;
                command_node *tail__ = n->tail;
                // move command node content to new node
                n->head = n->tail = NULL;
                command_node *nn = add_node(n,part.substr(ri),n->mask,n->hidden);
                nn->cmd = n->cmd;
                nn->head = head__;
                nn->tail = tail__;
                nn->start = n->start;
                // clean up current node
                n->mask |= mask_;
                n->cmd = NULL;
                n->hidden = hidden_;
                n->start = NULL;
            }
            else {
                // duplicate
                LC_LOG_VERBOSE("duplicate node");
                n->mask |= mask_;
            }

            return n;
        }
    }

    command_node *command_node::add_root(command::filter_t mask_, bool hidden_)
    {
        if (start == NULL) {
            start = new (std::nothrow) command_node();
            if (start != NULL) {
                start->mask = mask_;
                start->hidden = hidden_;
            }
        }
        return start;
    }

    void command_node::associate(command *cmd_) 
    { 
        assert(cmd == NULL); // to catch duplicate commands
        cmd = cmd_; 
    }

    void command_node::dump(size_t level)
    {
        if (LC_LOG_CHECK_LEVEL(debug::DEBUG)) {
            static std::string indent = "                                     ";
            LC_LOG_DEBUG("%s%s[%p/0x%08x/%s/%p]%s",
                level>0?indent.substr(0,level*2).c_str():"",
                part.empty()?"--ROOT--":part.c_str(),
                this,mask,hidden?"HIDDEN":"VISIBLE",cmd,
                start!=NULL?"==>":"");

            ++level;

            if (start != NULL)
                start->dump(level);

            command_node *n = head;
            while (n != NULL) {
                n->dump(level);
                n = n->next;
            }
        }
    }


    command_cursor::command_cursor(command_node *r, size_t r_idx) :
        root(r),root_idx(r_idx),idx(0) {}

    command_cursor::command_cursor(const command_cursor &n) :
        root(n.current()),idx(0)
    {
        if (n.S.empty())
            root_idx = n.root_idx + n.idx;
        else
            root_idx = n.idx;
    }

    bool command_cursor::command(command::filter_t mask, bool ignore_hidden) const
    {
        return valid() &&
               (current()->mask & mask) != 0 &&
               (!current()->hidden || ignore_hidden) &&
               current()->cmd != NULL &&
               (current()->cmd->mask & mask) != 0 &&
               (!current()->cmd->hidden || ignore_hidden);
    }

    bool command_cursor::subword(command::filter_t mask, bool ignore_hidden) const
    {
        return valid() &&
               (current()->mask & mask) != 0 &&
               (!current()->hidden || ignore_hidden) &&
               current()->start != NULL;
    }

    size_t command_cursor::current_length() const
    {
        if (S.empty()) {
            if (root != NULL && root_idx < root->part.length())
                return (root->part.length() - root_idx);
        }
        else {
            command_node *n = S.top();
            if (n != NULL)
                return n->part.length();
        }
        return 0;
    }

    char command_cursor::current_char() const
    {
        if (S.empty()) {
            if (root != NULL && root_idx < root->part.length())
                return root->part.at(root_idx + idx);
        }
        else {
            command_node *n = S.top();
            if (n != NULL && idx < n->part.length())
                return n->part.at(idx);
        }
        return 0;
    }

    void command_cursor::rewind()
    {
        command_node_stack_t empty;
        std::swap(S,empty);
        w.clear();
        idx = 0;
    }

    std::string command_cursor::remainder() const
    {
        if (S.empty()) {
            if (root != NULL && (root_idx + idx) < root->part.length())
                return root->part.substr(root_idx + idx);
        }
        else {
            command_node *n = S.top();
            if (n != NULL && idx < n->part.length())
                return n->part.substr(idx);
        }
        return "";
    }

    bool command_cursor::next()
    {
        // depth first search
        command_node *n = current();
        if (n == NULL)
            return false;

        /*
        if (S.empty())
            LC_LOG_VERBOSE("root[%p]@%zu; w[%s]",root,idx,w.c_str());
        else
            LC_LOG_VERBOSE("n[%p]@%zu; w[%s]",n,idx,w.c_str());
        */

        std::string rstr = remainder();
        if (!rstr.empty()) {
            //LC_LOG_VERBOSE("w + [%s]",rstr.c_str());
            w.append(rstr);
            idx += rstr.length();
            return true;
        }
        else if (n->head != NULL) {
            //LC_LOG_VERBOSE("push[%p;next=%p]",n->head,n->head->next);
            S.push(n->head);
            idx = 0;
            return true;
        }
        else {
            while (!S.empty() && n != NULL && n->next == NULL) {
                if (idx > 0 && idx <= w.length())
                    w.erase(w.length() - idx);

                //LC_LOG_VERBOSE("pop[%p;next=%p]",S.top(),S.top()->next);
                S.pop();

                if (S.empty()) {
                    if (root != NULL && root_idx < root->part.length())
                        idx = root->part.length() - root_idx;
                    else
                        idx = 0;
                }
                else {
                    if ((n = S.top()) != NULL)
                        idx = n->part.length();
                    else
                        idx = 0;
                }
            }

            if (idx > 0 && idx <= w.length())
                w.erase(w.length() - idx);

            idx = 0;
            if (S.empty() || n == NULL) {
                w.clear();
                return false;
            }

            n = n->next;
            //LC_LOG_VERBOSE("pop[%p;next=%p]",S.top(),S.top()->next);
            S.pop();

            if (n != NULL) {
                //LC_LOG_VERBOSE("push[%p;next=%p]",n,n->next);
                S.push(n);
                return true;
            }
        }
        return false;
    }

    bool command_cursor::next_root()
    {
        if (S.empty()) {
            if (root != NULL && root->start != NULL && (root_idx + idx) >= root->part.length()) {
                root = root->start;
                root_idx = 0;
                rewind();
                return true;
            }
        }
        else {
            command_node *n = S.top();
            if (n != NULL && n->start != NULL && idx >= n->part.length()) {
                root = n->start;
                root_idx = 0;
                rewind();
                return true;
            }
        }
        return false;
    }

    bool command_cursor::find(const std::string &search, command::filter_t mask, bool ignore_hidden)
    {
        if (search.empty())
            return false;

        size_t si = 0; // index into search (0..length)

        while (current() != NULL && si < search.length()) {
            char c = search.at(si);

            if (idx >= current_length()) {
                if (current()->head != NULL && (current()->mask & mask) != 0 && (!current()->hidden || ignore_hidden)) {
                    S.push(current()->head);
                    idx = 0;
                }
                else {
                    return false;
                }
            }
            else if (c == current_char() && (current()->mask & mask) != 0 && (!current()->hidden || ignore_hidden)) {
                ++si;
                ++idx;
                w += c;
            }
            else if (idx == 0) {
                if (S.empty()) {
                    return false;
                }
                command_node *n = S.top()->next;
                if (n != NULL) {
                    S.pop();
                    S.push(n);
                    idx = 0;
                }
                else {
                    return false;
                }
            }
            else {
                return false;
            }
        }

        return (current() != NULL && si >= search.length() && (current()->mask & mask) != 0 && (!current()->hidden || ignore_hidden));
    }

    command *command_set::add(const std::string &cmd_str, const char *name, command::filter_t mask_, bool hidden_)
    {
        return add(cmd_str,name,token::ID_NOT_SET,mask_,hidden_);
    }

    command *command_set::add(const std::string &cmd_str, token::id_t ID, command::filter_t mask_, bool hidden_)
    {
        return add(cmd_str,NULL,ID,mask_,hidden_);
    }

    command* command_set::add(const std::string &cmd_str, const char *name, token::id_t ID, command::filter_t mask_, bool hidden_)
    {
        if (name != NULL || ID != token::ID_NOT_SET) {
            // break command string into tokens
            std::auto_ptr<token> Tadd(libchars::lexer(cmd_str));
            if (Tadd.get() == NULL)
                return NULL;
            // make sure none of the tokens are quoted strings nor empty strings
            token *T = Tadd.get();
            while (T != NULL) {
                if (T->status & token::IS_QUOTED)
                    return NULL;
                if (T->value.empty())
                    return NULL;
                T = T->next;
            }
            // build sanitized version of command string
            T = Tadd.get();
            std::string cmd_str_sanitized(T->value);
            T = T->next;
            while (T != NULL) {
                cmd_str_sanitized += ' ';
                cmd_str_sanitized.append(T->value);
                T = T->next;
            }
            // add command to internal list
            dirty = true;
            command *c_new = new command(cmd_str_sanitized, name, mask_, ID, hidden_);
            LC_LOG_VERBOSE("set[%p] command[%s] = %p",this,cmd_str_sanitized.c_str(),c_new);
            if (C_list != NULL)
                c_new->next = C_list;
            C_list = c_new;
            return c_new;
        }
        return NULL;
    }

    command::command(const std::string &cmd_str_, const char *name_, filter_t mask_, token::id_t ID_, bool hidden_) :
        ID(ID_),cmd_str(cmd_str_),mask(mask_),hidden(hidden_),next(NULL)
    {
        if (name_ != NULL)
            name.assign(name_);
    }

    void command::set_help(const char *help_)
    {
        if (help_ != NULL)
            help.assign(help_);
    }

    parameter* command::add(const parameter &par_)
    {
        par.push_back(par_);
        return &par.back();
    }

    enum lex_inputs {
        X_WS  = 0, // input: whitespace
        X_A0  = 1, // input: printable
        X_Q   = 2, // input: quote char
        X_ESC = 3, // input: escape char: '\'
        X_EOL = 4, // input: end of line
    };

    enum lex_states {
        S_WS  = 0, // state: whitespace or equals sign
        S_TOK = 1, // state: token
        S_STR = 2, // state: string
        S_E1  = 3, // state: esc 0 (tok)
        S_E2  = 4, // state: esc 0 (str)
        S_EOL = 5, // end of line
    };

    enum lex_actions {
        A_SOT  = 0x10, // action: start of new token + add char to new token
        A_PUSH = 0x20, // action: add char to existing token
        A_EOT  = 0x40, // action: end of existing token
        A_EOTP = 0x80, // action: add char to existing token + end of token
    };

    static uint32_t lex_transitions[][X_EOL+1] =
    {
        // STATE: X_WS, X_A0, X_Q, X_ESC, X_EOL
        /* S_WS  */ {  S_WS, S_TOK|A_SOT, S_STR|A_SOT, S_E1|A_SOT, S_EOL, },
        /* S_TOK */ {  S_WS|A_EOT, S_TOK|A_PUSH, S_STR|A_SOT|A_EOT, S_E1|A_PUSH, S_EOL|A_EOT, },
        /* S_STR */ {  S_STR|A_PUSH, S_STR|A_PUSH, S_WS|A_EOTP, S_E2|A_PUSH, S_EOL|A_EOT, },
        /* S_E1  */ {  S_TOK|A_PUSH, S_TOK|A_PUSH, S_TOK|A_PUSH, S_TOK|A_PUSH, S_EOL|A_EOT, },
        /* S_E2  */ {  S_STR|A_PUSH, S_STR|A_PUSH, S_STR|A_PUSH, S_STR|A_PUSH, S_EOL|A_EOT, },
        /* S_EOL */ {  S_EOL, S_EOL, S_EOL, S_EOL, S_EOL, },
    };

    token *lexer(const std::string &str)
    {
        token *t_tail = NULL;
        token *t_head = NULL;
        size_t offset = 0, offset_start = 0;
        uint32_t state = S_WS;
        do {
            char c = (offset < str.length()) ? str.at(offset) : 0;
            lex_inputs x = X_WS;
            if (c == 0)
                x = X_EOL;
            else if (c == '\\')
                x = X_ESC;
            else if (c == '"')
                x = X_Q;
            else if (isspace(c) || c == '=')
                x = X_WS;
            else if (isprint(c))
                x = X_A0;

            uint32_t tr = lex_transitions[state][x];
            if ((tr & (A_PUSH|A_EOTP)) != 0) {
                // character part of existing token
            }
            if ((tr & (A_EOT|A_EOTP)) != 0) {
                // end of token (if not empty)
                if (offset > offset_start && offset_start < str.length()) {
                    token *T = new token;
                    T->status = token::IN_STRING;
                    if (state == S_STR || state == S_E2)
                        T->status |= token::IS_QUOTED;
                    T->offset = offset_start;
                    T->length = offset - offset_start;
                    if (tr & A_EOTP) ++T->length;
                    T->value = str.substr(T->offset,T->length);
                    if (t_head == NULL) {
                        t_head = t_tail = T;
                    }
                    else {
                        t_head->next = T;
                        t_head = T;
                    }
                }
            }
            if ((tr & A_SOT) != 0) {
                // new token
                offset_start = offset;
            }
            state = tr & 0x0f;
        } while (offset++ < str.length());

        return t_tail;
    }


    commands::commands(terminal_driver &d) :
        edit_object(libchars::MODE_COMMAND),
        edit(d),mask(0),
        remember(NULL),status(EMPTY),dirty(true),
        t_cmd(NULL), t_par(NULL),cmd(NULL),
        timeout(0) {}

    commands::~commands()
    {
        delete t_cmd;
    }

    const std::string commands::value() const
    {
        std::string str;
        if (length() > 0 && data() != NULL)
            str.assign(data(),length());
        return str;
    }

    void commands::lexer()
    {
        delete t_cmd;
        t_par = NULL;
        t_cmd = libchars::lexer(value());
    }

    size_t commands::render(size_t buf_idx, size_t limit, std::string &sequence)
    {
        parse();

        size_t idx = buf_idx, displayed = 0, r_offset = 0, r_length = 0;
        while (idx < length() && displayed < limit) {
            command_char &C = characters[idx++];
            if ((displayed + C.display_length) > limit)
                break;
            if (r_length == 0) {
                r_offset = C.render_offset;
                r_length = C.render_length;
            }
            else {
                r_length += C.render_length;
            }
            displayed += C.display_length;
        }

        if (r_length > 0) {
            // add token color if render has to start in the middle of a token
            command_char &C = characters[buf_idx];
            if (C.T != NULL && buf_idx > C.T->offset)
                sequence.append(color_str(C.color));
            sequence = rendered_str.substr(r_offset, r_length);
            sequence.append(color_str(COLOR_NORMAL)); // just in case the rest of the terminal gets messed up
        }
        else {
            sequence.clear();
        }

        return displayed;
    }

    token *commands::find_current_token(size_t &offset) const
    {
        LC_LOG_VERBOSE("find current token for idx=%zu",insert_idx);
        token *T = t_cmd;
        while (T != NULL && T->length > 0) {
            LC_LOG_VERBOSE("search token [%p/%s@%zu+%zu]",T,T->value.c_str(),T->offset,T->length);
            offset = (insert_idx - T->offset);
            if (insert_idx == (T->offset + T->length)) {
                // just beyond end of token; make sure next token does not start immediately after this one
                if (T->next == NULL || !(T->next->status & token::IN_STRING))
                    return T;
                if (T->next->offset > insert_idx)
                    return T;
            }
            else if (insert_idx >= T->offset && insert_idx < (T->offset + T->length)) {
                // inside token
                return T;
            }
            T = T->next;
        }

        return NULL;
    }

    void commands::parse()
    {
        //PROCESS: tokens(IN) -> match -> sort -> validate -> tokens(OUT)

        if (dirty) {
            lexer();
            dirty = false;

            LC_LOG_VERBOSE("t_cmd[%p], cmd[%p]", t_cmd, cmd);

            // find longest match on command tokens
            status = (t_cmd == NULL) ? EMPTY : NO_COMMAND;
            cmd = NULL;
            t_par = NULL;
            token *T = t_cmd;
            token *Tcmd = NULL;
            command_cursor ci(&root);
            while (T != NULL) {
                if (T->status & token::IS_QUOTED || T->value.empty() || !ci.find(T->value,mask,true)) {
                    if (Tcmd == NULL)
                        status = NO_COMMAND;
                    break;
                }
                T->ttype = token::COMMAND;
                status = PARTIAL_COMMAND;
                if (!ci.end())
                    break;
                if (ci.command(mask,true)) {
                    cmd = ci.current()->get();
                    Tcmd = T;
                    // continue search in case a longer match is found
                }
                if (!ci.next_root())
                    break;
                T = T->next;
            }

            if (cmd != NULL) {
                // sort tokens using command parameters
                t_par = Tcmd->next;
                status = sort();
                validate();
            }

            // tokens -> characters
            rendered_str.clear();
            characters.clear();
            characters.resize(length() + 1);
            size_t idx = 0, nchars = 0;
            bool command_tokens_seen = false;
            T = t_cmd;
            while (T != NULL) {
                if (T->status & token::IN_STRING) {
                    //TODO: if only one possible command completion: show completion in grey
                    command_colors_e t_color = COLOR_NORMAL;
                    if (T->ttype == token::COMMAND) {
                        command_tokens_seen = true;
                        if (cmd != NULL)
                            t_color = COLOR_VALID_COMMAND;
                        else
                            t_color = COLOR_PARTIAL_COMMAND;
                    }
                    else if (T->status & token::INVALID) {
                        t_color = COLOR_INVALID_ARGUMENT;
                    }
                    else if (T->ttype == token::UNKNOWN) {
                        if (command_tokens_seen && cmd == NULL)
                            t_color = COLOR_INVALID_COMMAND;
                        else if (!(T->status & token::PARTIAL_ARG) && (status == MISSING_VALUE || status == TOO_FEW_ARGS))
                            t_color = COLOR_INVALID_ARGUMENT;
                        else
                            t_color = COLOR_UNKNOWN_TOKEN;
                    }
                    else if (T->status & token::VALIDATED) {
                        if (T->status & token::IS_QUOTED)
                            t_color = COLOR_QUOTED_STRING;
                        else
                            t_color = COLOR_VALID_ARGUMENT;
                    }
                    else if (T->status & token::PARTIAL_ARG) {
                        t_color = COLOR_PARTIAL_ARGUMENT;
                    }
                    else {
                        t_color = COLOR_INVALID_ARGUMENT;
                    }

                    LC_LOG_VERBOSE("token:offset[%zu];length[%zu];str[%s]",T->offset,T->length,T->value.c_str());

                    // deal with whitespace before token
                    while (idx < T->offset) {
                        command_char &C = characters[idx++];
                        C.T = T;
                        C.color = COLOR_NORMAL;
                        C.display_offset = nchars;
                        C.display_length = 1;
                        C.cursor_pos = nchars++;
                        C.render_offset = rendered_str.length();
                        C.render_length = 1;
                        rendered_str += ' ';
                    }
                    // first character
                    {
                        command_char &C = characters[idx];
                        C.T = T;
                        C.color = t_color;
                        C.display_offset = nchars;
                        C.display_length = 1;
                        C.cursor_pos = nchars++;
                        C.render_offset = rendered_str.length();
                        if (T->length == 1) {
                            C.render_length = color_length(t_color) + color_length(COLOR_NORMAL) + 1;
                            rendered_str.append(color_str(t_color));
                            rendered_str += at(idx++);
                            rendered_str.append(color_str(COLOR_NORMAL));
                        }
                        else {
                            C.render_length = color_length(t_color) + 1;
                            rendered_str.append(color_str(t_color));
                            rendered_str += at(idx++);
                        }
                    }
                    // middle characters
                    while (idx < (T->offset + T->length - 1)) {
                        command_char &C = characters[idx];
                        C.T = T;
                        C.color = t_color;
                        C.display_offset = nchars;
                        C.display_length = 1;
                        C.cursor_pos = nchars++;
                        C.render_offset = rendered_str.length();
                        C.render_length = 1;
                        rendered_str += at(idx++);
                    }
                    // last character
                    if (T->length > 1) {
                        command_char &C = characters[idx];
                        C.T = T;
                        C.color = t_color;
                        C.display_offset = nchars;
                        C.display_length = 1;
                        C.cursor_pos = nchars++;
                        C.render_offset = rendered_str.length();
                        C.render_length = color_length(COLOR_NORMAL) + 1;
                        rendered_str += at(idx++);
                        rendered_str.append(color_str(COLOR_NORMAL));
                    }
                }
                T = T->next;
            }

            if (idx < length()) {
                // deal with whitespace at the end
                while (idx < length()) {
                    command_char &C = characters[idx++];
                    C.T = NULL;
                    C.color = COLOR_NORMAL;
                    C.display_offset = nchars;
                    C.display_length = 1;
                    C.cursor_pos = nchars++;
                    C.render_offset = rendered_str.length();
                    C.render_length = 1;
                    rendered_str += ' ';
                }
            }

            // add dummy character at the end (after whitespace)
            {
                command_char &C = characters[idx];
                C.T = NULL;
                C.color = COLOR_NORMAL;
                C.display_offset = nchars;
                C.display_length = 0;
                C.cursor_pos = nchars;
                C.render_offset = rendered_str.length();
                C.render_length = 0;
            }

            LC_LOG_VERBOSE("str[%s]",rendered_str.c_str());
        }
    }

    void commands::auto_complete()
    {
        LC_LOG_VERBOSE("complete@%zu/%zu",insert_idx,length());

        // only allowed at end of line + on command tokens (no quotes)
        while (insert_idx >= length()) {
            // sanity check on position in line + type of token
            size_t t_offset = 0;
            token *Tcur = find_current_token(t_offset);
            if (Tcur != NULL) {
                LC_LOG_VERBOSE("current token [%p/%s@%zu+%zu] @ %zu",Tcur,Tcur->value.c_str(),Tcur->offset,Tcur->length,t_offset);
                if (t_offset < Tcur->length || (Tcur->status & token::IS_QUOTED))
                    return;
            }
            else {
                LC_LOG_VERBOSE("** no current token **");
            }

            std::string available_str;
            token *T = t_cmd;
            while (T != NULL && T->length > 0) {
                if (T == Tcur) {
                    if (t_offset < Tcur->length) {
                        available_str.append(T->value, 0, t_offset);
                    }
                    else {
                        available_str.append(T->value);
                    }
                    break;
                }
                else {
                    available_str.append(T->value);
                    available_str += ' ';
                }
                T = T->next;
            }

            // find current position in command dictionary
            command_cursor ci(&root);
            T = t_cmd;
            bool available = true;
            while (T != NULL && ci.valid() && available) {
                LC_LOG_VERBOSE("search token [%p/%s@%zu+%zu]",T,T->value.c_str(),T->offset,T->length);
                if (T == Tcur) {
                    if (t_offset > 0) {
                        std::string v_search = T->value.substr(0,t_offset);
                        LC_LOG_VERBOSE("offset[%zu]; search for [%s]",t_offset,v_search.c_str());
                        available = ci.find(v_search,mask);
                    }
                    else {
                        available = false;
                    }
                    T = NULL;
                }
                else {
                    available = ci.find(T->value,mask);
                    available = available && ci.next_root();
                    T = T->next;
                }
            }

            LC_LOG_DEBUG("cursor: %p @ %zu: [%s]", ci.current(), ci.current_idx(), ci.word().c_str());
            if (ci.valid())
                LC_LOG_VERBOSE("dictionary option: [%s]",ci.current()->part.c_str());

            if (!available) {
                LC_LOG_DEBUG("** no options available **");
                return;
            }

            // collect combinations (full words + words with sub-words + words with valid commands)
            typedef std::list<std::string> string_list_t;
            string_list_t options;
            bool is_command = false;
            LC_LOG_VERBOSE("current partial word: [%s]",ci.word().c_str());
            command_cursor cw(ci);
            while (cw.next()) {
                if (!cw.word().empty() && cw.end() && (cw.command(mask) || cw.subword(mask))) {
                    LC_LOG_VERBOSE("options += %s[%s]", ci.word().c_str(), cw.word().c_str());
                    options.push_back(cw.word());
                    is_command = cw.command(mask);
                }
            }
            if (!options.empty() && ci.end() && (ci.command(mask) || ci.subword(mask))) {
                LC_LOG_VERBOSE("options += <cr>");
                options.push_back("");
            }

            if (options.empty()) {
                if (!ci.word().empty() && ci.end()) {
                    // end-of-word --> add space
                    LC_LOG_DEBUG("insert space");
                    insert(' ');
                    // no need to reparse because whitespace does not change token list
                }
                else {
                    return;
                }
            }
            else if (options.size() == 1) {
                // middle-of-word --> complete word (take first option)
                // line empty / after whitespace + 1 path --> add word
                LC_LOG_DEBUG("insert(middle/empty) [%s]",options.front().c_str());
                const std::string &word_to_insert = options.front();
                size_t i = 0;
                while (i < word_to_insert.size())
                    insert(word_to_insert.at(i++));
                // reparse before next iteration of loop
                parse();
                // stop auto-complete if new string is a valid command
                if (is_command)
                    return;
            }
            else {
                // 2+ options --> find longest common prefix amongst options
                std::string prefix = options.front();
                string_list_t::const_iterator si = options.begin();
                ++si;
                while (!prefix.empty() && si != options.end()) {
                    const std::string &cmp_str = *si++;
                    if (cmp_str.length() < prefix.length())
                        prefix = prefix.substr(0,cmp_str.length());
                    while (!prefix.empty() && cmp_str.compare(0,prefix.length(),prefix) != 0)
                        prefix.erase(prefix.length() - 1);
                }

                if (!prefix.empty()) {
                    // common prefix available --> add prefix
                    size_t i = 0;
                    while (i < prefix.size())
                        insert(prefix.at(i++));
                    // reparse before next iteration of loop
                    parse();
                }
                else {
                    // dump available options
                    printf("\n");
                    string_list_t::const_iterator si = options.begin();
                    while (si != options.end()) {
                        printf("%s%s%s%s%s\n",
                            color_str(COLOR_NORMAL),
                            available_str.c_str(),
                            color_str(COLOR_COMPLETION),
                            (si++)->c_str(),
                            color_str(COLOR_NORMAL));
                    }
                    rewind();
                }
                return;
            }
        }
    }

    void commands::show_help()
    {
        printf("?\n");

        //TODO: use terminal driver to print strings
        //TODO: pretty-print (e.g. start help in same column)

        switch (status) {
        case TERMINATED:
        case TIMEOUT:
        case FORCED_RETURN:
            // ignore
            break;
        case VALID_COMMAND:
        case MISSING_VALUE:
        case INVALID_ARG:
        case TOO_FEW_ARGS:
        case TOO_MANY_ARGS:
            // command known --> dump parameter help
            show_parameters();
            break;
        case NO_COMMAND:
            // invalid command --> cannot show help
            printf("No known commands match current line\n");
            break;
        case EMPTY:
        case PARTIAL_COMMAND:
            // empty / partially known --> find list of commands that match partial string
            if (cmd != NULL) {
                printf("** INTERNAL ERROR: partial command **\n");
            }
            else {
                std::string cmd_str_search;
                token *T = t_cmd;
                while (T != NULL && T->length > 0) {
                    LC_LOG_VERBOSE("search token [%p/%s@%zu+%zu]",T,T->value.c_str(),T->offset,T->length);
                    if (!cmd_str_search.empty())
                        cmd_str_search += ' ';
                    cmd_str_search.append(T->value);
                    if (T->next == NULL && insert_idx > (T->offset + T->length))
                        cmd_str_search += ' ';
                    T = T->next;
                }
                // build match list
                typedef std::list<command*> command_list_t;
                command_list_t match;
                unsigned int match_max_length = 0;
                command_sorted_list_t::const_iterator ci = C_sorted.begin();
                while (ci != C_sorted.end()) {
                    command *cmd = *ci++;
                    if ((cmd->mask & mask) != 0 && !cmd->hidden && (cmd_str_search.empty() || cmd->cmd_str.compare(0,cmd_str_search.length(),cmd_str_search) == 0)) {
                        match.push_back(cmd);
                        if (cmd->cmd_str.length() > match_max_length)
                            match_max_length = cmd->cmd_str.length();
                    }
                }
                // dump match list
                command_list_t::const_iterator mi = match.begin();
                while (mi != match.end()) {
                    const command *cmd = *mi++;
                    printf("%-*s : %s\n", match_max_length, cmd->cmd_str.c_str(), cmd->help.c_str());
                }
            }
            break;
        }

        rewind();
    }

    token *commands::find_flag(const char *name)
    {
        if (name == NULL)
            return NULL;
        if (cmd == NULL)
            return NULL;

        token *T = t_par;
        while (T != NULL && (T->name != name || T->ttype != token::FLAG))
            T = T->next;

        return T;
    }

    token *commands::find_key(const char *name)
    {
        if (name == NULL)
            return NULL;
        if (cmd == NULL)
            return NULL;

        token *T = t_par;
        while (T != NULL && (T->name != name || T->ttype != token::KEY))
            T = T->next;

        if (T != NULL) {
            if (T->next == NULL)
                return NULL; // {key,value} pair missing 'value'
            else if (T->next->ttype != token::KEY)
                return NULL; // {key,value} pair missing 'value'
            else if (!(T->next->status & token::IS_VALUE))
                return NULL; // {key,value} pair invalid 'value'
        }

        return T;
    }

    token *commands::find_pval(token *position)
    {
        if (cmd == NULL)
            return NULL;

        token *T = (position == NULL) ? t_par : position->next;
        while (T != NULL && T->ttype != token::VALUE)
            T = T->next;

        return T;
    }

    token *commands::find_arg(token::id_t ID)
    {
        if (cmd == NULL)
            return NULL;
        if (ID == token::ID_NOT_SET)
            return NULL;

        token *T = t_par;
        while (T != NULL && T->ID != ID)
            T = T->next;

        // special test case: {key,value} pair must have both components
        if (T != NULL && T->ttype == token::KEY) {
            if (T->next == NULL)
                return NULL; // {key,value} pair missing 'value'
            else if (T->next->ttype != token::KEY)
                return NULL; // {key,value} pair missing 'value'
            else if (!(T->next->status & token::IS_VALUE))
                return NULL; // {key,value} pair invalid 'value'
        }

        return T;
    }

    command_set &commands::cset(const std::string &set_name)
    {
        return (set_name.empty() ? cset() : C_sets[set_name]);
    }

    bool commands::cset_exists(const std::string &set_name)
    {
        return (!set_name.empty() && C_sets.find(set_name) != C_sets.end());
    }

    command_set &commands::cset()
    {
        C_set_default.activate(); // will stay activated after this point
        return C_set_default;
    }

    void commands::deactivate_all_sets()
    {
        //NOTE: C_set_default not deactivated
        command_sets_t::iterator csi = C_sets.begin();
        while (csi != C_sets.end()) {
            command_set &C_set = csi->second;
            C_set.deactivate();
            ++csi;
        }
    }

    void commands::dump_dictionary()
    {
        if (LC_LOG_CHECK_LEVEL(debug::DEBUG)) {
            build_commands();
            LC_LOG_DEBUG("Command dictionary tree:");
            root.dump();
        }
    }

    void commands::dump_commands()
    {
        if (LC_LOG_CHECK_LEVEL(debug::DEBUG)) {
            build_commands();
            command_sorted_list_t::const_iterator ci = C_sorted.begin();
            while (ci != C_sorted.end()) {
                command *cmd = *ci++;
                LC_LOG_DEBUG("[0x%08x/%s/%p] %s",cmd->mask,cmd->hidden?"HIDDEN":"VISIBLE",cmd,cmd->cmd_str.c_str());
            }
        }
    }

    void commands::dump_tokens()
    {
        if (LC_LOG_CHECK_LEVEL(debug::DEBUG)) {
            if (cmd != NULL) {
                LC_LOG_DEBUG("cmd[%p:%d:%s] t_par[%p]",cmd,cmd->ID,cmd->name.c_str(),t_par);
            }

            token *T = t_cmd;
            while (T != NULL) {
                char buffer[1024] = "";
                switch (T->ttype) {
                case token::UNKNOWN:
                    sprintf(buffer, " (%p:%s): unknown", T, T->value.c_str());
                    break;
                case token::COMMAND:
                    sprintf(buffer, " (%p:%s): command", T, T->value.c_str());
                    break;
                case token::VALUE:
                    sprintf(buffer, " (%p:%s): value", T, T->value.c_str());
                    break;
                case token::FLAG:
                    sprintf(buffer, " (%p:%s): flag", T, T->name.c_str());
                    break;
                case token::KEY:
                    if (T->status & token::IS_VALUE)
                        sprintf(buffer, " (%p:%s): pair-value", T, T->value.c_str());
                    else
                        sprintf(buffer, " (%p:%s): pair-key", T, T->name.c_str());
                    break;
                }
                if (T->status & token::IN_STRING)
                    sprintf(buffer+strlen(buffer), " [in-string]");
                if (T->status & token::INVALID)
                    sprintf(buffer+strlen(buffer), " [invalid]");
                if (T->status & token::VALIDATED)
                    sprintf(buffer+strlen(buffer), " [validated]");
                LC_LOG_DEBUG(buffer);
                T = T->next;
            }
        }
    }

    void commands::reset_status()
    {
        status = EMPTY;
        dirty = true;
        t_cmd = NULL;
        t_par = NULL;
        cmd = NULL;
    }

    void commands::add_commands_from_set(command_set &C_set)
    {
        LC_LOG_VERBOSE("set[%p] root[%p]",&C_set,&root);
        command *C_list = C_set.get();
        while (C_list != NULL) {
            command *cmd = C_list;
            C_list = C_list->next;
            // break command string into tokens
            std::auto_ptr<token> Tadd(libchars::lexer(cmd->cmd_str));
            if (Tadd.get() == NULL)
                continue;
            // add command word(s) to dictionary
            token *T = Tadd.get();
            command_node *cnode = root.add(T->value,cmd->mask,cmd->hidden);
            T = T->next;
            while (T != NULL && cnode != NULL) {
                if ((cnode = cnode->add_root(cmd->mask,cmd->hidden)) != NULL) {
                    cnode = cnode->add(T->value,cmd->mask,cmd->hidden);
                    T = T->next;
                }
            }
            // associate dictionary node with command
            if (cnode != NULL) {
                dirty = true;
                cnode->associate(cmd);
                C_sorted.insert(cmd);
            }
        }
    }

    void commands::build_commands()
    {
        // rebuild dictionary and sorted list if any of the command sets were modified
        bool rebuild = C_set_default.modified();
        command_sets_t::iterator csi = C_sets.begin(); 
        while (csi != C_sets.end()) { // run through all sets to clear modify flag, even if default set already indicates 'rebuild'
            command_set &C_set = csi->second;
            if (C_set.modified()) rebuild = true;
            ++csi;
        }
        if (rebuild) {
          root.clear();
          C_sorted.clear();
          add_commands_from_set(C_set_default);
          csi = C_sets.begin();
          while (csi != C_sets.end()) {
              add_commands_from_set(csi->second);
              ++csi;
          }
        }
    }

    void commands::enable_timeout(size_t timeout_s)
    {
        this->timeout = timeout_s;
    }

    void commands::disable_timeout()
    {
        this->timeout = 0;
    }
    
    commands::status_t commands::run(command::filter_t mask_)
    {
        build_commands();

        if (this->prompt.empty())
          this->prompt = ">";
        this->mask = mask_;

        if (!edit.interactive()) {
          parse();
          return status;
        }
        else {
          // reset command structures before starting new command string
          reset_status();

          while (true) {
              (void)edit.edit(*this,timeout);
              switch (edit.key()) {
              case SEQ_TIMEOUT:
                  return TIMEOUT;
              case FORCED_RET:
                  return FORCED_RETURN;
              case KEY_ENTER:
                  //TODO: remove quotes from strings
                  dump_tokens();
                  edit.newline();
                  rewind();
                  // record current line in history
                  if (remember != NULL) {
                      if (status != TERMINATED && status != EMPTY && remember != NULL && length() > 0)
                          remember->add(value());
                      else
                          remember->cancel();
                  }
                  return status;
              case KEY_TAB:
                  auto_complete();
                  break;
              case KEY_HELP:
                  if (insert_idx >= length())
                      show_help();
                  break;
              case KEY_QUIT:
                  edit.newline();
                  rewind();
                  return TERMINATED;
              case KEY_UP:
                  if (remember != NULL) {
                      if (!remember->searching())
                          remember->set(value());
                      if (remember->prev())
                          set(remember->current());
                  }
                  break;
              case KEY_DOWN:
                  if (remember != NULL && remember->searching() && remember->next())
                      set(remember->current());
                  break;
              case KEY_PGUP:
                  if (remember != NULL) {
                      if (remember->searching() && remember->search_idx() != insert_idx)
                          remember->cancel();

                      if (!remember->searching()) {
                          remember->set(value());
                          if (remember->search(insert_idx)) {
                              set(remember->current(),remember->search_idx());
                          }
                      }
                      else {
                          if (remember->search_prev()) {
                              set(remember->current(),remember->search_idx());
                          }
                      }
                  }
                  break;
              case KEY_PGDN:
                  if (remember != NULL) {
                      if (remember->searching() && remember->search_idx() != insert_idx)
                          remember->cancel();

                      if (remember->searching() && remember->search_next())
                          set(remember->current(),remember->search_idx());
                  }
                  break;
              default:
                  edit.newline();
                  rewind();
                  return EMPTY;
              }
          }
        }

        return EMPTY;
    }

}

