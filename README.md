## Little Kitten Web Server

A little web server written in C.

- Single threaded using I/O multiplexing (select)
- No external dependencies
- lklib and lknet code available to create your own http server or client
- Free to use and modify (MIT License)

## Usage

To compile:

    $ make lkws

To run:

    $ ./lkws <home directory>

Compiles and runs only on Linux (for now)

## Todo

- better name for 'lkws'?
- add support for cgi via cgi-bin directory
- add port parameter
- add logging
- add tests for lknet
- add perf tests for many simultaneous clients
- check for memory leaks (valgrind?)

