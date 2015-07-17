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

#ifndef __LIBCHARS_COMMANDS_H__
#define __LIBCHARS_COMMANDS_H__

#include "editor.h"
#include "parameter.h"
#include "history.h"

#include <string>
#include <stack>
#include <algorithm>
#include <vector>
#include <set>

namespace libchars {

    class command
    {
        friend class commands;
        friend class command_cursor;
        friend class command_set;
        friend struct command_sort_criteria;

    public:
        typedef uint64_t filter_t;
        const static filter_t UNLOCK_ALL = -1;

    public:
        command(const std::string &cmd_str, const char *name = NULL, filter_t mask = 1, token::id_t ID = token::ID_NOT_SET, bool hidden = false);
        ~command() { delete next; }

    public:
        token::name_t name;
        token::id_t ID;

    private:
        std::string cmd_str;
        std::string help;
        parameters_t par;
        filter_t mask;
        bool hidden;
        class command *next;

    public:
        void set_help(const char *help); // context-sensitive help on parameter

        parameter* add(const parameter &par);
    };

    class command_node
    {
        friend class command_cursor;

    public:
        std::string part;
        command::filter_t mask;
        bool hidden;

    private:
        class command *cmd;
        class command_node *head; // first child
        class command_node *tail; // last child
        class command_node *next; // next sibling
        class command_node *start; // next word

    public:
        command_node(const std::string &part_);
        command_node();
        ~command_node();

    private:
        void add_node(command_node *parent, command_node *n);
        command_node *add_node(command_node *parent, const std::string &part, command::filter_t mask, bool hidden = false);
        void dump(size_t level); //DEBUG

    public:
        void clear();

        command_node *add(const std::string &word, command::filter_t mask, bool hidden = false);
        command_node *add_root(command::filter_t mask, bool hidden = false);

        void associate(command *cmd_);

        inline class command *get() { return cmd; }

        inline void dump() { dump(0); }
    };

    class command_cursor
    {
    private:
        typedef std::stack<command_node*> command_node_stack_t;
        command_node_stack_t S; // [root] -> node1 -> ... -> nodeX (top of stack)
        std::string w; // starts empty; word from root@root_idx --> command_node@idx
        command_node *root; // base command_node, i.e. start of command_node tree
        size_t root_idx; // start index in root node
        size_t idx; // character index; on root node 0 = root_idx; on other nodes 0 = 0

    public:
        command_cursor(command_node *r = NULL, size_t r_idx = 0);
        command_cursor(const command_cursor &n);

        inline bool top() const { return (S.empty() && idx == root_idx); }
        inline command_node *current() const { return S.empty() ? root : S.top(); }
        inline size_t current_idx() const { return idx; }
        inline bool end() const { return remainder().empty(); }
        inline bool valid() const { return (current() != NULL); }
        inline const std::string &word() const { return w; }

        bool command(command::filter_t mask, bool ignore_hidden = false) const;
        bool subword(command::filter_t mask, bool ignore_hidden = false) const;

        size_t current_length() const;
        char current_char() const;

        void rewind();

        std::string remainder() const;

        bool next();

        bool next_root();

        bool find(const std::string &search, command::filter_t mask, bool ignore_hidden = false);
    };

    struct command_sort_criteria
    {
        bool operator() (const command* lhs, const command* rhs) const
        {
            if (lhs != NULL && rhs != NULL)
                return lhs->cmd_str < rhs->cmd_str;
            else
                return false;
        }
    };

    enum command_colors_e {
        COLOR_NORMAL,
        COLOR_UNKNOWN_TOKEN,
        COLOR_VALID_COMMAND,
        COLOR_PARTIAL_COMMAND,
        COLOR_INVALID_COMMAND,
        COLOR_COMPLETION,
        COLOR_QUOTED_STRING,
        COLOR_VALID_ARGUMENT,
        COLOR_PARTIAL_ARGUMENT,
        COLOR_INVALID_ARGUMENT,
    };

    struct command_char
    {
        const token *T;         // token linked to character
        command_colors_e color; // color index for token
        size_t display_offset;  // excludes color escape codes
        size_t display_length;  // length of displayed string for this character; excludes color escape codes
        size_t cursor_pos;      // relative to start of render string; excludes color escape codes
        size_t render_offset;   // relative to start of render string
        size_t render_length;   // includes color escape codes
    };

    typedef std::vector<command_char> command_chars;

    token *lexer(const std::string &str);

    class command_set
    {
    public:
        command_set() : C_list(NULL),active(false),dirty(false) {}
        ~command_set() { delete C_list; }
    private:
        command *C_list;
        bool active;
        bool dirty;
    public:
        command *add(const std::string &cmd_str, const char *name, command::filter_t mask = 0x0001, bool hidden = false);
        command *add(const std::string &cmd_str, token::id_t ID, command::filter_t mask = 0x0001, bool hidden = false);
        command *add(const std::string &cmd_str, const char *name, token::id_t ID, command::filter_t mask = 0x0001, bool hidden = false);

        inline command *get() { return active ? C_list : NULL; }

        inline void activate() { if (!active) { active = true; dirty = true; } }
        inline void deactivate() { if (active) { active = false; dirty = true; } }

        inline bool modified() { if (dirty) { dirty = false; return true; } else return false; } // clear-on-read
    };

    class commands : public edit_object
    {
    public:
        commands(terminal_driver &d);
        ~commands();

    public:
        typedef enum {
            VALID_COMMAND = 0,  // command found + arguments validated
            EMPTY,              // no tokens
            NO_COMMAND,         // no match in command tree
            PARTIAL_COMMAND,    // partial match in command tree
            MISSING_VALUE,      // command found, value missing from {key,value} pair
            INVALID_ARG,        // command found, 1+ arguments failed validation
            TOO_FEW_ARGS,       // command found, not enough arguments specified
            TOO_MANY_ARGS,      // command found, too many arguments specified
            TERMINATED,         // command entry terminated with ctrl^C
            TIMEOUT,            // terminal timeout; 
        } status_t;

    private:
        void lexer();

        virtual void set(const char *line,size_t idx = std::string::npos) { edit_object::set(line,idx); dirty=true; }
        virtual void insert(const char c) { edit_object::insert(c); dirty=true; }
        virtual void del() { edit_object::del(); dirty=true; }
        virtual void bksp() { edit_object::bksp(); dirty=true; }
        virtual void wipe() { edit_object::wipe(); dirty=true; }
        virtual void swap() { edit_object::swap(); dirty=true; }

        virtual size_t render(size_t buf_idx, size_t limit, std::string &sequence);

        status_t sort();

        token *find_current_token(size_t &offset) const;

        void parse();
        void validate();
        void auto_complete();
        void show_help();
        void show_parameters();
        void reset_status();

        inline void emptied() { reset_status(); }

        void add_commands_from_set(command_set &C_set);
        void build_commands();

    private:
        editor edit;

        typedef std::set<command*,command_sort_criteria> command_sorted_list_t;
        command_sorted_list_t C_sorted;

        typedef std::map<std::string,command_set> command_sets_t;
        command_sets_t C_sets;
        command_set C_set_default; // set "0"

        command_node root;
        command::filter_t mask;
        history *remember;

        status_t status;
        bool dirty;
        token* t_cmd; // first token in linked-list (aka first command token)
        token* t_par; // first parameter token (only set if command found)
        command *cmd; // command (if found during search in tokens)

        std::string rendered_str;
        command_chars characters;
        size_t timeout;

    public:
        const char *color_str(command_colors_e color_idx) const;

        inline size_t color_length(command_colors_e color_idx) const { return strlen(color_str(color_idx)); }

        inline void clear_screen() { edit.clear_screen(); }

        command_set &cset(const std::string &set_name); // add command set if not found
        bool cset_exists(const std::string &set_name);

        command_set &cset(); // default command set; always active (once used)

        void deactivate_all_sets(); // except default set

        void dump_dictionary(); //DEBUG; call after loading commands
        void dump_commands(); //DEBUG; call after loading commands
        void dump_tokens(); //DEBUG; call after run()

        inline void use(history *h) { remember = h; }

        inline void load(const std::string &cmdline) { set(cmdline.c_str()); }

        void enable_timeout(size_t timeout_s = 10);
        void disable_timeout();
    
        status_t run(command::filter_t mask = command::UNLOCK_ALL); // editor --> command + arguments (validated)

        inline command *get() { return cmd; } // only valid after call to run()

        // argument lookup method 1: raw token linked-list
        inline token *args() { return t_par; }

        // argument lookup method 2: by token ID
        token *find_arg(token::id_t ID); // find arguments based on ID (only valid IDs allowed)

        // argument lookup method 3: by type + name/position
        token *find_flag(const char *name); // test (ret)->status contains FLAG_SET
        token *find_key(const char *name); // value = (ret)->next
        token *find_pval(token *position = NULL); // find next positional value after 'position'
    };
}

#endif // __LIBCHARS_COMMANDS_H__
