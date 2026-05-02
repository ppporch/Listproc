#!/usr/princeton/bin/perl -s

# by Michael R. Gettes, Princeton University, 1994

# To use this program:
#	get the LIST file on to the system that ListProc is running
#	Run this program as UID server
#	Get internet.listing file from CREN (to LISTSERV@BITNIC,
#		issue command GET INTERNET LISTING). This file
#		allows you to map BITNET nodenames to Internet
#		names. Those BITNET names that cannot be mapped
#		will be %-hacked using the $interbit variable
#		as the INTERBIT gateway.
#
#	options: -L		- does not require internet.listing file
#		 -il=filename	- specify location of internet.listing file
#		 -interbit=<addr> - specify alternate hostname of interbit
#				gateway. We default to interbit.cren.net
#		 -pw=<pw>	- specify password for all subscribers
#		 -noack		- by default, if a user has no options set
#				  we will set their option to SET MAIL ACK.
#

$[=1;

$LOCAL = 0;
$LOCAL = 1 if defined($L);		# -L means do NOT do interbit processing
$interbit = "interbit.cren.net" unless defined($interbit);
$pw = time() unless defined ($pw);	# specify -pw to override default pw
$mailack = 'ACK';
$mailack = 'NOACK' if defined($noack);	# Say -noack for default of No Mail Ack

$prog = $0;
die "$prog: Must specify listname where .subscribers file is created\n"
	unless defined(@ARGV[1]);
$listname = shift(@ARGV);

die "LPDIR environment not set\n" unless defined($ENV{'LPDIR'});
$LPDIR = $ENV{'LPDIR'};

$listname =~ tr/a-z/A-Z/;
$ListDIR = "$LPDIR/lists/$listname";
die "List $listname not configured\n" unless -d "$ListDIR";

# first we need to read in the ./internet.listing file
# use -il= to specify alternate location of internet.listing

if (! $LOCAL) {
	$il = "./internet.listing" unless defined($il);
	open(IL,"$il") || die "open: $il: $!\n";
	while (<IL>) {
		next unless /^\s/o;
		s/^\s//;
		($bitnode, $intnode) = split;
		next if $bitnode =~ /^$/o;
		$bitnet{$bitnode} = $intnode;
	}
	close(IL);
}

unlink("$ListDIR/.subscribers");
open(SUBFILE,">$ListDIR/.subscribers") || die "open: $ListDIR/.subscribers: $!\n";
warn("Converting LISTSERV subscribers to\n\tListProc format in $ListDIR/.subscribers...\n");

while (<>) {
	next if /^\*/o;
	next if /^$/o;
	$sub = substr($_,1,80);
	$ack = substr($_,81,1);		# ACK (A), NOACK (a), MSGACK (M)
	$mail = substr($_,82,1);	# MAIL (M), NOMAIL (m), DIGEST(D)
	$files = substr($_,83,1);	# FILES (F), NOFILES (f)
	$repro = substr($_,84,1);	# NOREPRO ( ), REPRO (R)
	$fullhdr = substr($_,85,1);	# SHORTHDR ( ), FULLHDR (F)
	$conceal = substr($_,92,1);	# NOCONCEAL ( ), CONCEAL (C)
	$yy = substr($_,86,2);		# we don't use this info
	$ddd = substr($_,88,3);		# we don't use this info
	($addr, @name) = split(/[ \t\n]+/, $sub);
	$name = join(' ',@name);
	$mailtype = "";
	$mailtype = 'DIGEST' if $mail eq "D";
	$mailtype = 'ACK' if ($mail !~ /[Dm]/) && ($ack =~ /[AM]/);
	$mailtype = 'POSTPONE' if ($mail eq "m");
	$mailtype = $mailack if $mailtype eq "";
	$concealtype = "NO";
	$concealtype = "YES" if $conceal eq "C";

# break down the net address into user/domain
# if domain does not contain a period, then we look it up
# in internet listing file to change the domain properly.
# if this does not exist, then we route it through interbit (unless -L)

	($user,$domain) = $addr =~ /(.*)\@(\S+)$/;
	if ($domain !~ /\./) {
		$domain = $bitnet{$domain} if defined($bitnet{$domain});
	}
	if (!$LOCAL && $domain !~ /\./) {
		$user = sprintf("%s%%%s.bitnet", $user, $domain);
		$domain = $interbit;
	}
	$addr = sprintf("%s@%s", $user, $domain);
	print SUBFILE "$addr $mailtype $pw $concealtype $name\n";
}
close(SUBFILE);
warn("Done.\n");
exit(0);
