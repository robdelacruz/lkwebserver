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

    lkws [homedir] [port] [host] [-f configfile] [--cgidir=cgifolder]

    configfile = configuration file containing site settings
                 see sample configuration file below
    homedir    = absolute or relative path to a home directory
                 defaults to current working directory if not specified
    port       = port number to bind to server
                 defaults to 8000
    host       = IP address to bind to server
                 defaults to localhost
    Examples:
    lkws ./testsite/ 8080
    lkws /var/www/testsite/ 8080 127.0.0.1
    lkws /var/www/testsite/ --cgidir=cgi-bin
    lkws -f sites.conf

Sample configuration file:

    serverhost=127.0.0.1
    port=5000

    # Matches all other hostnames
    hostname *
    homedir=/var/www/testsite
    alias latest=latest.html

    # Matches http://localhost
    hostname localhost
    homedir=/var/www/testsite

    # http://littlekitten.xyz
    hostname littlekitten.xyz
    homedir=/var/www/testsite
    cgidir=cgi-bin
    alias latest=latest.html
    alias about=about.html
    alias guestbook=cgi-bin/guestbook.pl
    alias blog=cgi-bin/blog.pl

    # http://newsboard.littlekitten.xyz
    hostname newsboard.littlekitten.xyz
    proxyhost=localhost:8001

    # Format description:
    #
    # The host and port number is defined first, followed by one or more
    # host config sections.
    #
    # The host config section always starts with the 'hostname <domain>'
    # line followed by the settings for that hostname. The section ends
    # on either EOF or when a new 'hostname <domain>' line is read,
    # indicating the start of the next host config section.


Compiles and runs only on Linux (sorry, no Windows version... yet)

## Todo

- add logging
- add perf tests for many simultaneous clients

