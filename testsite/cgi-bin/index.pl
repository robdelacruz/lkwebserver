#!/usr/bin/perl

my $starthtml = <<"EOT";
<!DOCTYPE html>
<html>
<head><title>perl cgi test</title></head>
<body>
<h1>Perl CGI Test</h1>
EOT

my $endhtml = <<"EOT";
</body></html>
EOT

print $starthtml;
print "<p>Hello from perl script.</p>\n";
print "<p>Bye from perl script.</p>\n";
print $endhtml;

