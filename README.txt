Copyright (C) 2013-2019 Roelof Nico du Toit.

libchars - Command Line Interface (CLI) Framework

Version 0.8.3 (25-Apr-2019)

See LICENSE.txt and all source files for license agreement.

Build instructions
==================
The build steps depend on availability of the cmake tools.  Refer to the
following link for steps to install cmake:
http://www.cmake.org/cmake/help/install.html

- mkdir libchars.build
- cd libchars.build
- cmake path-to-libchars-source
- make

Overview
========
The libchars package is an extensible C++ framework for building a command-line
interface (CLI) in Linux, FreeBSD, or OS/X.  It features a terminal driver, 
line editor, argument validation engine, commands engine and command history.
The code is distributed under the MIT license.

Dependencies
============
One of the design goals was to have as few dependencies as possible.  The only
dependency in the current version is using cmake at compile time to facilitate
building the binaries.  There are no runtime dependencies other than the C/C++
standard libraries.

The code compiles without issues when using the following compilers:
- clang version 4.1 (OS/X)
- gcc version 4.6.3 (Ubuntu 12.04LTS)

The VT100 terminal driver was validated using the following terminal clients:
- xterm
- iTerm (OS/X)
- Terminal (OS/X)
- ssh (OS/X, FreeBSD, Linux)
- Putty (Windows)
- NPort Serial to Ethernet appliance

Features
========
The CLI framework has support for the following features:
- Password entry mode (no echo of characters).
- Multi-line entry mode for large multi-line strings.
- Support editing strings that are larger than the terminal (auto-scroll).
- Override rendering of string in editor, e.g. add color or extra characters.
- Support for different types of parameters (flags, key-value, positional).
- Support for dynamically changing command list after every command.
- Command auto-completion and listing of command alternatives.
- Listing command parameters (if command is known).
- Context sensitive help on commands and parameters.
- Colorized tokens to distinguish invalid, valid, and partial commands.
- Colorized tokens to highlight invalid arguments.
- Colorized tokens to highlight quoted strings.
- Command history, with option to extend how history is made persistent.
- Command history search (up and down) based on partial string.
- Command argument validation, with extensible option types.
- Command prompt can be changed dynamically.
- Re-render line when terminal size changes.
- Support for hidden commands (not in auto-complete or command list)
- Non-interactive command parsing.
- Command sets, which can be used to implement command levels.
- Timeout on command editor; used for housekeeping before editing continues

Known Issues
============
- The command list is currently sorted alphabetically when displaying context-sensitive help, but some applications require displaying the list in the order commands were added.
- There is currently no wide-character / UTF8 support.
- The library is mostly thread-safe, but the caller must currently ensure that only one editor instance at a time is using the terminal driver.

Feature Requests
================
- Support for paging in terminal driver, i.e. pause on full page of output.
- Reference code for full shell implementation.
- Built-in argument validators for common types, e.g. IPv4 address, hostname.
- Support for positional argument groups for lists of arguments of the same type, e.g. lists of names.
- Option to display command list in the order commands were added.

Code Layout
===========
The libchars CLI framework is written in C++, and all classes are in
the libchars namespace.  The source files have names that reflect the
functional building blocks:

terminal.h/cpp     VT100 Terminal Driver
editor.h/cpp       Line editor and key sequence mapper
parameter.h/cpp    Tokens and command parameters
commands.h/cpp     Lexer and command parser
history.h/cpp      Command history, including history search
validation.h/cpp   Command argument validation
debug.h/cpp        Debug helper API; printf() style logs
test_editor.cpp    Sample application to demonstrate editing and rendering
test_commands.cpp  Sample application to demonstrate commands engine

Commands Engine
===============
The CLI framework commands engine supports the following parameter types:
- Key-Value pair, e.g. “name=John”
- Flags, e.g. “disable-color”
- Positional arguments, which are values indexed by position

The command parser supports entering the parameters in any order, apart from
positional arguments which should still be in order relative to each other,
but flags and key-value pairs may be entered anywhere in the command line.
Parameters are optional or mandatory – setting a default value on a key-value
pair or positional argument automatically makes it optional.  Flags are always
optional because the default value of a flag is false.  Parameters may also be
hidden, and default values may be specified.

Commands have unique identifiers:  a name and/or a unique ID.  At least one of
name or ID must be specified. Commands also have an optional 64-bit bitmask
that may be used to filter out certain commands based on an input mask, in
effect dynamically controlling the command list on every command. 

The command string may contain multiple words, e.g. “set color” or
“show attribute list”.  Commands may overlap, e.g. the command1 = “throw ball”
and command2 = “throw balls”.  Another example of overlap:
command1 = “throw ball” and command2 = “throw ball back”.

Commands are validated as you type, and the color of the command string
changes depending on the command status, e.g. partial commands are rendered
in yellow while a full match on a command string renders it in green.
Invalid commands or command parameters are rendered in red.

On return from the commands engine the command arguments may be extracted by
parameter ID, by parameter name, by parameter position, or by walking the
token list chain.

