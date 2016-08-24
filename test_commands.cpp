/*
Copyright (C) 2013-2015 Roelof Nico du Toit.

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

#include <assert.h>

using namespace libchars;

static command::filter_t __mask = command::UNLOCK_ALL & ~0x10000;

enum {
    VTYPE_COLOR = validator::USER,
    VTYPE_ANGLE,
};

struct validate_color : public validator
{
    virtual status_t check(const std::string &value) const
    {
        if (value=="red" || value=="white" || value=="blue")
            return validator::VALID;
        else if (value.compare(0,std::string::npos,"red",value.length()) == 0)
            return validator::PARTIAL;
        else if (value.compare(0,std::string::npos,"white",value.length()) == 0)
            return validator::PARTIAL;
        else if (value.compare(0,std::string::npos,"blue",value.length()) == 0)
            return validator::PARTIAL;
        else
            return validator::INVALID;
    }
} __v_color;

struct validate_angle : public validator
{
    virtual status_t check(const std::string &value) const
    {
        unsigned int x = strtoul(value.c_str(),NULL,0);
        return (x <= 90) ? validator::VALID : validator::INVALID;
    }
} __v_angle;

static void load_commands(commands *cmds)
{
    command *c = NULL;
    parameter *p = NULL;

    c = cmds->cset().add("exit",100,command::UNLOCK_ALL); assert(c != NULL);
    c->set_help("Exit application");
    c = cmds->cset().add("clear",101,command::UNLOCK_ALL); assert(c != NULL);
    c->set_help("Clear screen");

    command_set &C_set1 = cmds->cset("LEVEL1");
    C_set1.activate();

    c = C_set1.add("abcde",1000,command::UNLOCK_ALL,true); assert(c != NULL);
    c = C_set1.add("abc"  ,1001,command::UNLOCK_ALL,true); assert(c != NULL);
    c = C_set1.add("a"    ,1002,command::UNLOCK_ALL,true); assert(c != NULL);

    c = C_set1.add("g    ",1010,command::UNLOCK_ALL,true); assert(c != NULL);
    c = C_set1.add("ghi  ",1011,command::UNLOCK_ALL,true); assert(c != NULL);
    c = C_set1.add("ghijk",1012,command::UNLOCK_ALL,true); assert(c != NULL);

    c = C_set1.add("uvwyz",1020,1,true); assert(c != NULL);
    //c = C_set1.add("uvwyz",1021,2,true); assert(c != NULL); <-- this will trigger abort

    c = C_set1.add("throw ball",1); assert(c != NULL);
    c->set_help("Rapidly transport ball to remote location");
    p = c->add(parameter(1,"angle",VTYPE_ANGLE)); assert(p != NULL);
    p->set_help("Angle (0-90)");
    p->set_default("45");
    p = c->add(parameter(2,"hard")); assert(p != NULL);
    p->set_help("Put some extra effort into it");
    p = c->add(parameter(3,validator::NONE)); assert(p != NULL);
    p->set_help("Name of person receiving the ball");
    p = c->add(parameter(4,validator::NONE)); assert(p != NULL);
    p->set_help("What to shout after throwing the ball");
    p->set_optional();
    p->set_hidden();

    c = C_set1.add("throw ball back",5,0x0002); assert(c != NULL);
    c->set_help("Return ball to original position");

    c = C_set1.add("throw balls","many",2,0x0003); assert(c != NULL);
    c->set_help("Transport many balls to remote location");

    c = C_set1.add("throw-away",3,0x0003); assert(c != NULL);
    c->set_help("Throw ball away");

    c = C_set1.add("set ball","set",9); assert(c != NULL);
    c->set_help("Modify attributes of ball");
    p = c->add(parameter(1,"color",VTYPE_COLOR)); assert(p != NULL);
    p->set_help("Color (red|white|blue)");
    p = c->add(parameter(2,"fast")); assert(p != NULL);
    p->set_help("Only select fast balls");
    p = c->add(parameter(3,validator::NONE)); assert(p != NULL);
    p->set_help("Brand name");
    p->set_default("ACME");
    c = C_set1.add("set ball none","set-none",99); assert(c != NULL);
    c->set_help("Get rid of ball");

    c = C_set1.add("show statistics",10); assert(c != NULL);
    c->set_help("Dump statistics about throws");

    c = C_set1.add("unlock special",200,command::UNLOCK_ALL,true); assert(c != NULL);
    c->set_help("Unlock hidden commands");
    c = C_set1.add("use special command",201,0x10000); assert(c != NULL);
    c->set_help("Do something with new knowledge");

    c = C_set1.add("enter level",500,command::UNLOCK_ALL); assert(c != NULL);
    c->set_help("Enter new level of commands");

    command_set &C_set2 = cmds->cset("LEVEL2");

    c = C_set2.add("hello",501,command::UNLOCK_ALL); assert(c != NULL);
    c->set_help("Say hi");
    c = C_set2.add("return",502,command::UNLOCK_ALL); assert(c != NULL);
    c->set_help("Return to previous level");
}

int execute_command(commands *cmds)
{
    command *c = cmds->get();
    printf("Execute[%p:%s:%d]\n",c,c->name.c_str(),c->ID);

    switch (c->ID) {
    case 1:
        {
            printf("-- throw ball --\n");
            token *T = NULL;
            if ((T = cmds->find_key("angle")) != NULL)
                printf("%d:%s=%s\n",T->ID,T->name.c_str(),T->next->value.c_str());
            if ((T = cmds->find_flag("hard")) != NULL)
                printf("%d:%s=TRUE\n",T->ID,T->name.c_str());
            if ((T = cmds->find_pval(NULL)) != NULL) {
                printf("%d:arg=%s\n",T->ID,T->value.c_str());
                if ((T = cmds->find_pval(T)) != NULL) {
                    printf("%d:arg=%s\n",T->ID,T->value.c_str());
                }
            }
        }
        break;
    case 2:
        printf("-- throw balls --\n");
        break;
    case 3:
        printf("-- throw-away --\n");
        break;
    case 5:
        printf("-- throw ball back --\n");
        break;
    case 9:
        {
            printf("-- set ball --\n");
            token *T = NULL;
            if ((T = cmds->find_arg(1)) != NULL)
                printf("1:%s=%s\n",T->name.c_str(),T->next->value.c_str());
            if ((T = cmds->find_arg(2)) != NULL)
                printf("2:%s=TRUE\n",T->name.c_str());
            if ((T = cmds->find_arg(3)) != NULL)
                printf("3:brand=%s\n",T->value.c_str());
        }
        break;
    case 10:
        printf("-- show statistics --\n");
        break;
    case 99:
        printf("-- set ball none --\n");
        break;
    case 100:
        return 1;
    case 101:
        cmds->clear_screen();
        break;
    case 200:
        printf("Hidden commands unlocked\n");
        __mask |= 0x10000;
        break;
    case 201:
        printf("-- use special command --\n");
        break;
    case 500:
        printf("-- activate new level --\n");
        cmds->cset("LEVEL1").deactivate();
        cmds->cset("LEVEL2").activate();
        break;
    case 501:
        printf("-- hello world --\n");
        break;
    case 502:
        printf("-- return to previous level --\n");
        cmds->cset("LEVEL1").activate();
        cmds->cset("LEVEL2").deactivate();
        break;
    default:
        printf("Invalid command ID\n");
    }

    return 0;
}

int main(int argc, char *argv[])
{
    if (argc > 1 && argv[1][0] == 'd')
        LC_LOG_SET_LEVEL(libchars::debug::DEBUG);
    if (argc > 1 && argv[1][0] == 'v')
        LC_LOG_SET_LEVEL(libchars::debug::VERBOSE);
    else
        LC_LOG_SET_LEVEL(libchars::debug::DISABLED);

    if (argc > 2)
        __mask = strtoul(argv[2],NULL,0);

    terminal_driver &tdriver = terminal_driver::initialize();
    commands cmds(tdriver);

    //TODO: load & parse history file
    history remember;
    cmds.use(&remember);

    // add user-defined types to validator
    int ret;
    validation &vv = validation::initialize();
    ret = vv.add_validator(VTYPE_COLOR, &__v_color);  assert(ret == 0);
    ret = vv.add_validator(VTYPE_ANGLE, &__v_angle);  assert(ret == 0);

    // add commands & parameters
    load_commands(&cmds);
    cmds.dump_dictionary();
    cmds.dump_commands();

    // get command (+history +completion)
    bool running = true;
    cmds.enable_timeout(5);
    cmds.set_return_timeout(15); // <-- one-shot timeout
    while (running) {
        commands::status_t ret = cmds.run(__mask);
        switch (ret) {
        case commands::VALID_COMMAND:
            if (execute_command(&cmds) != 0)
                running = false;
            break;
        case commands::EMPTY:
            printf("No tokens\n");
            break;
        case commands::NO_COMMAND:
            printf("No match in command tree\n");
            break;
        case commands::PARTIAL_COMMAND:
            printf("Partial match in command tree\n");
            break;
        case commands::MISSING_VALUE:
            printf("Command found, value missing from {key,value} pair\n");
            break;
        case commands::INVALID_ARG:
            printf("Command found, 1+ arguments failed validation\n");
            break;
        case commands::TOO_FEW_ARGS:
            printf("Command found, not enough arguments specified\n");
            break;
        case commands::TOO_MANY_ARGS:
            printf("Command found, too many arguments specified\n");
            break;
        case commands::TERMINATED:
            // cancel current command
            break;
        case commands::FORCED_RETURN:
        case commands::TIMEOUT:
            //NOTE: must call cmds.clear() if screen modified (e.g. printf) before calling cmds.run() again
            break;
        }
        if (ret != commands::TIMEOUT && ret != commands::FORCED_RETURN)
            cmds.clear();
    }

    tdriver.shutdown(); // not strictly required; done here to clean up before logging disabled

    return 0;
}
