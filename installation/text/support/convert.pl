#!/usr/local/bin/perl
#
#	Modified from the original file by,
#	Marco Hernandez 
#
# The convert.pl program will convert as much as possible information from
# a Listproc 6.0c server to Listproc 7.1 format.  
# 
# Tom Remmers
# University of Washington
# 
# 
# Input Files
# -----------
# 
# config.old		From ~server home directory.
# owners.old		From ~server home directory.
# aliases.old		From /usr/lib/aliases or equivalent.
# 
# 
# Output Files
# ------------
# 
# requests.tosubmit	File to append to L7's ~server/requests
# aliases.toadd		File to append to systems /usr/lib/aliases.
# 
# 
# Config File Format Requirements
# -------------------------------
# 
# The config.old file must not have any "&" continuation characters for 
# any of the specific list lines.
# 
# In the server line(s) in the config.old file:
# 
# - There only is "server listaddress" on the first line.
# - Subsequent lines must begin with "-a" or "-c" in the first two columns.
# - All private and concealed lists must be on consecutive lines, eg.
#   no comments or blank lines.
# - Private and concealed lists must be on the same line as the 
#   associated "-a" or "-c" flag.
# 
# 
# Improvements Over Distribution Code 
# -----------------------------------
# 
# The improvements refer to the requests file that is given to listproc to 
# initialize all lists.
# 
# - Creates one requests file that can be appended to ~server/requests
# - Creates one aliases file that can be appended to /usr/lib/aliases
# - Does all lists at once
# - Parses concealed and private, rather than the interactive prompt
# - Does not add "moderated-edit" for each list
# - Corrects minor syntax errors
# - Parses list "-P" and "-f" flags
# - Grabs "-m" flags from old /usr/lib/aliases file
# - Automatically preserves LISTSERV automatic archive format:
#   listname.logyymm (you should alter this if you want a different format).
# - Can specify different mail alias pipe program, eg. Catmail
# 
# 
# To Do/Flaws
# -----------
# 
# - Does not remove invalid puncuation from comment field (" %[^<>`*?|,\n]")
# - Does not parse "&" continuation lines in 6.0 config file
# - Does not parse owners CC options 
# - Does not parse disable lines from 6.0 config file
# - Does not parse header lines, but it really doesn't need to.
# - Does poor job of parsing mail defaults, you would have to modify 
#   commented code below to suit your particular format.
# 
# Caveats
# -------
# 
# The only error checking occurs when listproc processes the request file.
# Since I'm only running the program once, the modifications are not as
# thorough as they could be.  Others are encouraged to improve this program.
#
# If you have many lists, make sure to increase the limit_message parameter
# in the config file if needed:
#
# limit message 512000    # reject messages longer than 65536 bytes

###########################################################################
#  Define local input information.  You WILL need to modify some of these!

$managerpassword = "manager password";
$lpdirectory="/usr/local/listproc";
$configfile="config.old";
$ownersfile="owners.old";
$oldaliasesfile="aliases.old";
$listmanadd="manager@u.washington.edu";
$catmail="catmail";

# Note!! Uncomment the "no-reflector" parameter below if you want the
# list comment to appear in the outgoing "To:" field.
# Also, go to the /^default/ section below and modify the code to suit
# your own 6.0 format if you want to parse mail defaults 
##########################################################################

#  Output files
$initfile=">"."requests.tosubmit";
$newaliasesfile=">"."aliases.toadd";

# Create array of lists, concealed lists, and private lists 
# from old config file

open(CONFIG,"$configfile") || die "Can't open $configfile $!\n";
while (<CONFIG>) {
        if (/^list/) {
           @listline=split(' ',$_);
           @list_array[$list_ind++]=@listline[1];
        }
	if (/^server/) {
	   while (<CONFIG>) {
	      if (/^-/) {
	         @listline=split(' ',$_);
	         $number_of_elements=@listline;
	         for($i=0; $i < $number_of_elements-1; $i=$i+2) {
	            if ($listline[$i] =~ "a") {
	               @private_array[$private_ind++]=@listline[$i+1];
	            }
	            if ($listline[$i] =~ "c") {
	               @conceal_array[$conceal_ind++]=@listline[$i+1];
	            }
	         }
	      }
	      else {
	         last;
	      }
	   }
	}
}

# Open input/output files for big crunch
open(INIT,$initfile) || die "Can't open $initfile $!\n";
open (ALIASFILE,$newaliasesfile) || die "Can't open $newaliasesfile $!\n";

# Create mail header
print INIT "From $listmanadd\n\n";

# Big loop through all lists in lists file. 
foreach $listname (@list_array) {

# reset flags
$validlist=0;
$send_by_all=0;
$reply_to_sender=0;
$moderated_edit=0;
$forward_rejects=0;
$list_ind=0;
$private_ind=0;
$conceal_ind=0;

# Go back to start of old config file
close(CONFIG);
open(CONFIG,"$configfile") || die "Can't open $configfile $!\n";

# Loop through old config file for info about list
# NOTE:  No indent here
while (<CONFIG>) {
	if (/^list/) {
	   @listline=split(' ',$_);
		if(@listline[1] eq $listname) {
			$validlist=1;
			$listpassword=@listline[4];
			$listaddress=@listline[2];
			$listowner=@listline[3];
			print INIT "init ",$managerpassword," ",$listname," ",$listaddress," &\n";
			print INIT "password ",$listpassword,",&\n";
			$number_of_elements=@listline;
			$start=5;
			if($number_of_elements > $start) {
			   for($i=$start;$i < $number_of_elements;$i++) {
				if ($listline[$i] =~ "s") {
				   $send_by_all=1;
				}
				if ($listline[$i] =~ "p") {
				   $reply_to_sender=1;
				}
				if ($listline[$i] =~ "P") {
				   $reply_to_sender=1;
				}
				if ($listline[$i] =~ "f") {
				   print INIT "forward-rejects,&\n";
				}
				if ($listline[$i] =~ "M") {
			  	   print INIT "moderated-edit ",$listowner,",&\n";
				}
			    }
			}
		}
	}
}
if ($validlist eq 0) {
	print "ERROR:  could not find ",$listname," in the config file \n";
	exit 1;
}
if ($reply_to_sender) {
	print INIT "reply-to-sender,&\n";
}
else
{
	print INIT "reply-to-list-always,&\n";
}
#	If uncomment next line, it will do comment thing
#	print INIT "no-reflector,&\n";

print ALIASFILE $listname,":\"|",$lpdirectory,"/",$catmail," -L \U$listname\E -f\"\n";
print ALIASFILE $listname."-request",":\"|",$lpdirectory,"/",$catmail," -L \U$listname\E -fo\"\n";
print ALIASFILE "owner-".$listname,":\"|",$lpdirectory,"/",$catmail," -L \U$listname\E -fe\"\n";

seek(CONFIG,0,0);
while(<CONFIG>) {
	if (/^archive/) {
	   @archiveline=split(' ',$_);
		if(@archiveline[1] eq $listname) {
			print INIT "archive ",$lpdirectory,"/archives/",$listname," ",$listname,".log%y%m,&\n";
		}
	}
	if (/^ceiling/) {
		@ceilingline=split(' ',$_);
		if(@ceilingline[1] eq $listname) {
		  print INIT "message-limit ",$ceilingline[2],",&\n";
		}
	} 
	if (/^comment/) {
		@commentparse=split(' ',$_);
		if(@commentparse[1] eq $listname) {
			chop(@commentparse=split('#',$_));
			print INIT "comment \"",@commentparse[1],"\",&\n";
		}
	}

#  Fix this according to your own format.  As is, it parses,
#
#  default list {
#    mail = ack
#  }
#
#	if (/^default/) {
#		@defaultline=split(' ',$_);
#		if(@defaultline[1] eq $listname) {
#                	$_ = <CONFIG>;
#			@mailline=split(' ',$_);
#			print INIT "default mail ",$mailline[2],",&\n";
#		}
#	}
	if(/^digest/) {
		@digestline=split(' ',$_);
		if(@digestline[1] eq $listname) {
		   if($digestline[3] < 168) {
			print INIT "digest daily 23:59,&\n";
		   }
		   if($digestline[3] > 168 && @digestline[3] < 336) {
			print INIT "digest weekly Sunday,&\n";
		   }
		   if($digestline[3] > 336) {
			print INIT "digest monthly,&\n";
		   }
		}     
	}
		
}

open(OWNERS,$ownersfile) || die "Can't open $ownersfile: $!\n";
while (<OWNERS>) {
	if(substr($_,0,1) ne "#") {
	  @inline=split(' ',$_);
		if(@inline[1] eq  $listname) {
	  		print INIT "owners ",@inline[0],",&\n";
		}	
	}
}


open(ALIASES,"$oldaliasesfile") || die "Can't open $oldaliasesfile $!\n";
while (<ALIASES>) {
	@listline=split(' ',$_);
        $listmatch="$listname:";
	if(@listline[0] eq $listmatch) {
		$number_of_elements=@listline;
		$start=3;
		if($number_of_elements > $start) {
		   for($i=$start;$i < $number_of_elements;$i++) {
			if ($listline[$i] =~ "m") {
                           print INIT "moderated-no-edit ",$listowner,",&\n";
			}
                   }
		}
        }
}

$listupp = "\U$listname";
$private_list=0;
$concealed_list=0;

foreach $concealed (@conceal_array) {
	if($concealed eq $listupp) {
	   $concealed_list=1;
	   print INIT "hidden-list,&\n";
	}	
}
if ($concealed_list eq 0) {
	print INIT "visible-list,&\n";
}

foreach $private (@private_array) {
	if($private eq  $listupp) {
	   $private_list=1;
           print INIT "owner-subscriptions,&\n";
	}	
}
if ($private_list eq 0) {
	print INIT "open-subscriptions,&\n";
}

if ($send_by_all) {
	print INIT "send-by-all\n\n";
}
else
{
	print INIT "send-by-subscribers\n\n";
}

# End of foreach loop through the lists
}



