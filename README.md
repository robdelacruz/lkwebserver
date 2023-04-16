## Little Kitten Web Server

A little web server written in C for Linux.

- No external library dependencies
- Single threaded using I/O multiplexing (select)
- Supports CGI interface
- lklib and lknet code available to create your own http server or client
- Free to use and modify (MIT License)

## Usage

To compile:

    $ make lkws

Usage:

    lkws [homedir] [port] [host] [-a <alias1>=<path>]...
    homedir = absolute or relative path to a home directory
              defaults to current working directory if not specified
    port    = port number to bind to server
              defaults to 8000
    host    = IP address to bind to server
              defaults to localhost

    Examples:
    lkws ./testsite/ 8080 -a latest=latest.html -a about=about.html
    lkws /var/www/testsite/ 8080 127.0.0.1 -a latest=folder/latest.html
    lkws /var/www/testsite/ --cgidir=cgifolder

Compiles and runs only on Linux (sorry, no Windows version... yet)

## Todo

- add logging
- add tests for lknet
- add perf tests for many simultaneous clients
- check for memory leaks (valgrind?)

