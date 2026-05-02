#!/usr/bin/perl

#
#
#			   filter00.pl  Version 3.0
#
#		    Copyright (c) 1999  ThePorchDotCom, LLC
#	       Original idea by Phillip Porch (ppp@theporch.com)
#	    Written by Stephen Modena, AB4EL (shimshon@theporch.com)
#			    All rights reserved.
#
#    This program is free software; you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by 
#    the Free Software Foundation; either version 2, or (at your option) any
#    later version, or
#
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See either
#    the GNU General Public License or the Artistic License for more details.
#
#    You should also have received a copy of the GNU General Public License
#    along with this program; if not, write to the Free Software Foundation,
#    Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
#
#    Contact information:
#    Phillip Porch (ppp@theporch.com)
#    Stephen Modena (shimshon@theporch.com)
#
#
#
#	Usage: This program is designed to be called in a pipe.
#	I have placed it in my sendmail alias file such that the
#	message is piped through it before going to the mailing list
#	software program.
#	
#  testlist: "|/usr/local/bin/filter00.pl |/usr/server/catmail -L TESTLIST -f"
#
#	Please see the helpfile.txt which is included with this distribution
#	for more information about this program.
#
#*********************modification history********************
#  "filter00.pl  Version 1.0" became "filter00.pl  Version 2.0"
#           by change of message text in "sub trashCanMsg(@)"
#			by streamlining to more methods/functions
#*************************************************************
#  "filter00.py  Version 2.0" became "filter00.py  Version 3.0"
#            by addition of lastBoundary routines
#			by allowing "multipart/digest" to get through
#*************************************************************

use strict;

#
# let's avoid run away files : pick your favorate length
# this number is exclusive of the length of the email header
#
my $MAX_MSG_LINE_COUNT = 200;
#my $MAX_MSG_LINE_COUNT = 35;
#
#
my $headerSize = 0;		# length of email header in case we need it later
my $i=0;				# general purpose counter
my $str ="";			# scratch string
my @work=();			# main work buffer
#
#	here we go!
open(INFILE,"-");
open(OUTFILE, ">-");
#
# bring the mail header in
#
while( ($_=<INFILE>) !~ /^ *\n/ ) { push @work,$_; }	# stop at first blank line
push @work,$_;											# don't lose that last line read
$headerSize = $#work;									# remember length of mail header
#
# find the content type line
#my $contentTypeStr="";
#foreach (@work) {
#	if( /^Content-Type:/i ) { $contentTypeStr = $_; }
#	}
my $contentTypeStr = getContentTypeIfItExists(\@work);
#
my $boundary = getBoundaryIfItExists(\@work);
my $lastBoundary = getLastBoundaryIfItExists($boundary);
#
getRestOfFile(\@work);
#
if($contentTypeStr =~/text\/plain/)	{
		print OUTFILE @work;	# send the mail msg to output
		close INFILE;
		close OUTFILE;
		exit(0); # cleanly
		}
##
#
# if execution continue past this point
#	it means we are dealing with a MIME email
#
# ! ! scan work buffer until we hit a !(Content-Type: text/plain;)
#		or the end of the buffer
#
my $cycle = 0;
$i = $headerSize;
while( ($cycle==0) && ($i <= $#work) )	
		{
		if( $work[$i] =~ /^Content-Type:/i )	
				{   # found one
				if( 	($work[$i] !~ /text\/plain/i )	#content not plain text
						&&
						($work[$i] !~ /message\//i )	#content not message text
						&&
						($work[$i] !~ /multipart\/digest/i )	#content not multipart/digest text
					) #close if 
						{
						splice @work,$i;	# wipe out tail of work buffer
						trashCanMsg(\@work);
						push @work, "\n".$lastBoundary."\n\n";
						$cycle++;				
						}
				}
		$i++;
		}
print OUTFILE @work;	# send the mail msg to output
close INFILE;
close OUTFILE;
exit(0); # cleanly	# all done!
#
#
#
sub getRestOfFile(@)
{
	my $work = shift;
	# either --------------- block long files (uncomment the next 2 lines)
	#	while( ($_=<INFILE>) && ($i<$MAX_MSG_LINE_COUNT)) { push @{$work},$_; $i++; }
	#	if( $i>=$MAX_MSG_LINE_COUNT )	{ messageTooLong(\@{$work}); }
	# or --------------------- (uncomment the next line)
		push @{$work},<INFILE>;	# very fast - all file lengths are OK
	# end of either-or ---------------
}
#
#
#
sub messageTooLong(@)
{
	my $array = shift;
	push @{$array}, "* * * * * * * * * * * * * * * * * * * *\n";
	push @{$array}, "*  MESSAGE EXCEEDED PERMITTED LENGTH  *\n";
	push @{$array}, "*                                     *\n";
	push @{$array}, "*          message truncated          *\n";
	push @{$array}, "*                                     *\n";
	push @{$array}, "* * * * * * * * * * * * * * * * * * * *\n";
}
#
#
#
sub trashCanMsg(@)
{
	my $array = shift;
	push @{$array}, "Content-Type: text/plain; charset=us-ascii\n";
	push @{$array}, "Content-Transfer-Encoding: 7bit\n";
	push @{$array}, "\n";
	push @{$array}, "* * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\n";
	push @{$array}, "*         ---REMAINDER OF MESSAGE TRUNCATED---            *\n";
	push @{$array}, "*     This post contains a forbidden message format       *\n";
	push @{$array}, "*  (such as an attached file, a v-card, HTML formatting)  *\n";
	push @{$array}, "*    Mail Lists at theporch.com only accept PLAIN TEXT    *\n";
	push @{$array}, "* If your postings display this message your mail program *\n";
	push @{$array}, "* is not set to send PLAIN TEXT ONLY and needs adjusting  *\n";
	push @{$array}, "* * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\n";
}
#
#
#
sub getContentTypeIfItExists(@)
{
	my $work = shift;
	my $contentTypeStr="";
	foreach (@work) {
		if( /^Content-Type:/i ) { $contentTypeStr = $_; }
		}
	return $contentTypeStr="";
}
#
#
#
sub getBoundaryIfItExists(@)
{
	my $work = shift;
	my $header = "";
	my $boundary = "";

	foreach(@{$work}) { $header .= $_; }

	my $target0 = ".*Content-type:.*boundary *= *\".*\".*";
	my $target1 = ".*Content-type:.*boundary *= *\"(.*)\".*";

	if( $header =~ /$target0/is ) { 
		$boundary = $header;
		$boundary =~ s/$target1/$1/is;
		}
	return $boundary;
}
#
#
#
sub getLastBoundaryIfItExists($) 
{
	my $boundary = shift;
	my $lastBoundary = "--";
	if( length($boundary)>0 ) {
		$lastBoundary .=  $boundary;
	}
	$lastBoundary .= "--";
	return $lastBoundary;
}
#
#
#
