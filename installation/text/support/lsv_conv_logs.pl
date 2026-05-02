#!/usr/princeton/bin/perl -s

# by Michael R. Gettes, Princeton University, 1994

# To use this program:
#
#	lsv_conv_logs.pl <listname>.log*
#
#	This will convert all LISTSERV log files to ListProc
#	format. The files will be archived (the list must already
#	be defined and an archive directory specified) in the
#	appropriate location in compressed form. You must run this
#	program as UID server. I suggest you create a new directory,
#	get on your VM system, FTP the log files of the lists you
#	wish to move and then come back here and run this program.

# 	There are no options to this program


$debug=0 unless defined($debug);
$[=1;
$prog = $0;
$LSV_fence = "=" x 73;
$Zopt = "-Z";		# no compression by default;
$Zopt = "" if defined($Z);	# if -Z; then user wants compression

die "LPDIR environment not set\n" unless defined($ENV{'LPDIR'});
$LPDIR = $ENV{'LPDIR'};

die "Specify LISTSERV logfiles as arguments\n" if $#ARGV==0;

# We presume that the filenames of the log-files will be passed
# to this script. These filenames are in LSV format which is 
# LISTNAME.LOGXXXXX. We will use the listname to deposit the LOGXXXXX
# in the right place as well as convert the log file to mbox format.

foreach (@ARGV) {
	$lsv_logfile = $_;
	($listname,$logname,@rest) = split(/[\.]+/, $lsv_logfile);
	die "filename $lsv_logfile does not conform to LISTSERV logfile format\n"
		if ($listname eq "") || ($logname eq "");
	($ulistname = $listname) =~ tr/a-z/A-Z/;
	$logname =~ tr/A-Z/a-z/;
	$ListDIR = "$LPDIR/lists/$ulistname";
	die "List $listname not configured\n" unless -d "$ListDIR";
	die "LISTSERV logfile $lsv_logfile not readable\n" unless -r "$lsv_logfile";
	# go and find archive location if it exists
	open(CONFIG,"$ListDIR/config") || die "open: $ListDIR/config: $!\n";
	$archive_dir = "";
	while (<CONFIG>) {
		next unless /^archive/oi;
		@rest = split;
		$archive_dir = sprintf("-d %s", @rest[3]);
		last;
	}
	close(config);
	warn("Converting $ulistname $logname...\n");
	&LSVtoMbox();
}
exit(0);

sub LSVtoMbox {

	open(LSVLOG,"$lsv_logfile") || die "open: $lsv_logfile: $!\n";
	$mbox_logfile = "mbox";
	unlink("$mbox_logfile");
	open(MBOX,">$mbox_logfile") || die "open: $mbox_logfile: $!\n";
	$in_header = 1;
	@headers = ();
	$in_body = 0;
	$mail_sep = "";
	while (<LSVLOG>) {
		if ( ($in_body) && (!/^$LSV_fence$/o) ) {
			s/^/>/ if /^From /o;
			print MBOX $_;
		}
		next unless /^$LSV_fence$/o || ($in_header && ! $in_body);
		$in_body = 0;
		$in_header = 0 if /^\s*$/o;
		next if /^$LSV_fence$/o;
		push(@headers, $_),next if $in_header;

		# All headers are now in @headers, get from address

		$from = "";
		foreach (@headers) {
			next unless ( $from ne "" ) || (/^from:/oi);
			($from = $_, next) if /^from:/oi;
			last if !/^\s/o;
			$from = sprintf("%s %s", $from, $_);
		}
		# we have from field, get address
		$from =~ s/\n//g;
		if ($from =~ /\<(\S+)\>/) {
			($addr = $from) =~ s/.*\<(.*)>.*/$1/;
		} else {
			($addr = $from) =~ s/\S+\s+(\S+)/$1/;
		}
		# now, get and fix the date
		($date) = grep(/^Date:/, @headers);
		($x, $day, $dd, $mon, $year, $time, @rest) = split(/[ \t\n]+/,$date);
		$day =~ s/,//g;
		$year = sprintf("19%s", $year) if length($year) == 2;
		$newdate = sprintf("%s %s %s %s %s", $day, $mon, $dd, $time, $year);
		$mbox_from = sprintf("From %s %s\n", $addr, $newdate);
		$headers = join('',@headers);
		printf(MBOX "%s%s%s\n", $mail_sep, $mbox_from, $headers);
		$in_body = 1;
		$in_header = 1;
		$mail_sep = "\n";
		@headers = ();
	}
	close(MBOX);
	close(LSVLOG);
	unless ($debug) {
		rename($mbox_logfile, $logname);
		system("farch $Zopt -n -u -a $listname $archive_dir -D \"$ulistname List Archives\" $logname");
		unlink("$logname");
	}
}
