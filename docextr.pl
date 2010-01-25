#!/usr/bin/perl -w

my $printing = 0;

while (<>) {
	if ($printing) {
		if (/\/\*/) {
			print "!Comment in comment at $ARGV:$.\n";
		}
	}

	if (/\/\*_?\s+\(([a-z]*)\)/) {
		$printing = 1;
		print "#line $ARGV:$. ($1)\n";
	}
	if ($printing) {
		print ">" . $_;
	}
	if (/\*\//) {
		$printing = 0;
	}
} continue {
	close ARGV  if eof;
}
