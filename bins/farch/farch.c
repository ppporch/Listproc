/*
 			farch.c	6.8 CREN 97/03/01

			Copyright (c) 1993-98 by
	the Corporation for Research and Educational Networking (CREN)

All rights reserved. This software comprises confidential
information of CREN and may not be used, copied or made available to anyone,
except in accordance with the license under which it is furnished.


*/



/*
  ----------------------------------------------------------------------------
  |                	    FILE ARCHIVING UTILITY			     |
  |                                                                          |
  |                               Version 4.0				     |
  ----------------------------------------------------------------------------

  farch archives files into the specified directories, possibly splitting
  them into smaller parts. It then updates the DIR file of the appropriate
  archive. Split files have numbers appended to their original names;
  non-split files have their names intact. Binary files are uuencoded before
  processed, and all output files are compressed.

  COMMAND LINE OPTIONS:
    -n: Do not split input files.
    -t: Tar all input files.
    -b: Input files are binary; uuencode.
    -B: Input files are binary; do not uuencode, do not split.
    -s: Set the maximum size of each of the split subparts in kilobytes.
    -d: Specify the actual directory that the archived files will reside.
    -p: Specify the password for the private archive.
    -D: Specify a descriptive string to be appended to each DIR file.
    -r: Remove the specified files from the specified archive.
    -a: Specify the archive where the input files will be archived.
    -R: Remove the specified archive and all of its subarchives.
    -A: Same as -a but mark the archive for AFD/FUI usage.
    -Z: Turn of file compression.
    -u: Update existing file.
    -K: Don't lock.  Only used for recursive calls...

  WARNING: The program can also archive directory files, if these can be
  of any use.

  Algorithm:
  {
    Process command line arguments; allocate buffer_size
    Open master archive and all subarchives as necessary to get to the
      specified (path-to-) archive; get path to specified archive
    Verify existsence of target directory -- create it if necessary
    Tar files, if so requested
    for (each input file) {
      open
      get filesize
      if (-r)
        remove file
      else {
        if (-b)
	  uuencode file
        if (-n or filesize <= buffer_size) { -- Do not split --
	  if (-b)
	    copy uuencoded file into target directory
	  if (file does not exist in target directory)
	    copy it to target directory
	    compress it
	  update (archive's directory)
        }
        else split file {
	  while (not eof) {
	    get unique output filename
	    fwrite (leftover bytes from previous chunk)
	    fread (new chunk)
	    if (! -b) {
	      locate nearest newline from the end of the buffer
	      copy those skipped bytes to leftover
	    }
	    fwrite (new modified chunk)
	    compress it
	  }
	  fwrite (any leftovers)
	  compress part
	  update (archive's directory)
        }
      }
    }
  }
*/

#include <stdio.h>
#include <sys/types.h>
#if defined (bsdi)
# include <sys/malloc.h>
#elif defined (freebsd) 
# include <stdlib.h> 
#elif !defined (__convex__) && !defined (__NeXT__) && !defined (apollo) \
 && !defined (sequent) && !defined (unknown_port)
# include <malloc.h>
#endif
#ifndef unknown_port
# ifndef __NeXT__
#  include <unistd.h>
# else
#  include <libc.h>
# endif
#endif
#include <sys/stat.h>
#include <string.h>
#include <errno.h>
#ifdef unknown_port
extern int errno;
#endif
#include <signal.h>
#include "lplib/defs.h"
#include "lplib/struct.h"
#include "lplib/lpglobals.h"
#if defined (__NeXT__) || defined (unknown_port)
# include "next.h"
#endif
#ifndef NO_IPC_SUPPORT
# include <sys/ipc.h>
#endif

#include "port/sysdefs.h"
#include "lputil/lpdir.h"
#include "lputil/lplog.h"
#include "lputil/lpinit.h"
#include "lputil/lplock.h"
#include "lputil/lpfile.h"
#include "lputil/lpsyslib.h"


#define DEFAULT_SIZE    64 * 1024

#ifdef __STDC__
# include <stdarg.h>
extern int  syscom (char *, ...);
extern int  sysexec (char *, char *, char *, BOOLEAN, char *, BOOLEAN, BOOLEAN, ...);
extern int  sysexecv (char *, char *, char *, BOOLEAN, char *, BOOLEAN, BOOLEAN, char **);
extern char *tsprintf (char *, ...);
#else
# include <varargs.h>
extern int  syscom ();
extern int  sysexec ();
extern int  sysexecv ();
extern char *tsprintf ();
#endif
extern char *extract_filename (char *);
extern char *locase (char *);

/* 
#ifndef __NeXT__
extern long int atol (char *);
#else
extern long int atol (const char *);
#endif
*/
extern int  _getopt (int, char **, char *);

extern BOOLEAN make_indexes(char *, char *, char *, char *, int, int, BOOLEAN);
extern BOOLEAN mkdir1 (char *, char *, int);

extern char *mystrdup (char *);
extern int  otoi (char *);
extern int mv (char *, char *);
extern int cp (char *, char *);
extern char *Tempnam (char *, char *);

int   main (int , char **);
void   uuencode (FILE **, char *, char *, struct stat *, BOOLEAN);
void   rmarch (char *);
int    update (char *, char *, long int, long int *, char *, char *,
	       struct stat, BOOLEAN, BOOLEAN, BOOLEAN);
void   remove_files (char *, char *, long int, struct stat, BOOLEAN);
void   usage (char *, int);
int    do_archive (int, int, char **, char *, char *, char *, char *, char *,
		   long int, BOOLEAN, BOOLEAN, BOOLEAN, BOOLEAN, BOOLEAN,
		   BOOLEAN);
int    get_archive (char **, char *, char *, BOOLEAN, BOOLEAN, BOOLEAN);
int    get_dir (char *, char **, char *);
int    tar_files (int, int, char **, char *);
void   rmfiles (int, int, char **, char *, BOOLEAN, BOOLEAN);
int    gexit (int);
int    sighandle (int);

char *uue_file, *comment;
int  errno_tmp;



int main (int argc, char **argv)
{
  long int size = DEFAULT_SIZE;
  char *buf, *arch = NULL, *newdir = NULL, *dir, fullpath[1024],
       *tarf, *password = NULL, *text = "", *s;
  char *options = "nbBs:a:A:d:t:p:D:urZUR:wG:S:K";
  int c, ok = 1;
  BOOLEAN bin = FALSE, tar = FALSE, split = TRUE, uue = FALSE,
    remove_files = FALSE, no_compression = FALSE, updatef = FALSE,
    afd_fui = FALSE, remove_arch = FALSE, warning = TRUE;
  extern char *optarg, *getenv();
  extern int optopt, optind;
  bool get_lock = TRUE;


  /*
   * Some useful initializations
   */
  lpinit(argv[0],NULL);


  prog = argv[0];
  if (!(buf = (char *) malloc (size * sizeof (char))))
    fprintf (stderr, "\nmain(): malloc() failed for 1 byte.\n"),
    exit (EINVAL);
  if (!(dir = (char *) malloc (sizeof (char))))
    fprintf (stderr, "\nmain(): malloc() failed for 1 byte.\n"),
    exit (EINVAL);
#ifdef __linux__
  optind = 1;
#endif
  while ((c = _getopt (argc, argv, options)) != EOF)
    switch ((char) c) {
	case 'K': get_lock = FALSE; break;
    case 'n': split = FALSE; break;
    case 't': tar = TRUE; tarf = optarg;
    case 'b': bin = TRUE; uue = TRUE; break;
    case 'B': bin = TRUE; uue = FALSE; split = FALSE; break;
    case 's': size = (long) atol (optarg) * 1024;
      	      if (size <= 0)
                fprintf (stderr, "How am I supposed to allocate %ld bytes?\n",
                         size),
                exit (EINVAL);
              free ((char *) buf);
              if ((buf = (char *) malloc (size * sizeof (char))) == NULL)
                fprintf (stderr, "Size %d too big; not enough memory.\n", size),
                exit (EINVAL);
              break;
    case 'a': arch = locase (optarg); break;
    case 'A': arch = locase (optarg); afd_fui = TRUE; break;
    case 'd': newdir = optarg;
              if (*newdir != '/')
                fprintf (stderr, "Must specify full path for the directory.\n"),
                exit (EINVAL);
              break;
    case 'p': password = locase (optarg); break;
    case 'D': text = optarg; break;
    case 'r': remove_files = TRUE; break;
    case 'Z': no_compression = TRUE; break;
    case 'u': updatef = TRUE; break;
    case 'U': usage (argv[0], 0);
    case 'R': arch = locase (optarg); 
      if (!strcmp (arch, DEFAULT_ARCHIVE))
	fprintf (stderr, "Cannot remove default archive.\n"),
	exit (EINVAL);
      remove_arch = TRUE;
      break;
    case 'w': warning = FALSE; break;
#ifndef NO_IPC_SUPPORT
    case 'G': fprintf(stderr,"farch: -G is no longer used."); break;
    case 'S': fprintf(stderr,"farch: -S is no longer used."); break;
#endif
    case ':': fprintf (stderr, "Option '%c' requires an argument.\n", optopt);
              exit (EINVAL);
    case '?':
    default:
      fprintf (stderr, "Unrecognized option '%c'.\n", optopt);
      usage (argv[0], EINVAL);
    }

#ifdef bsd
  sigsetmask (0);
#endif
  signal (SIGINT, (void (*)()) gexit);
  if (!remove_arch && optind == argc && warning)
    fprintf (stderr, "WARNING: %s: filename(s) missing.\n", argv[0]);


  if(get_lock) lpl_lock(LPL_WRITE,LPL_GLOBAL_ARCHIVES,NULL);
  if ((ok &= get_archive (&arch, fullpath, password, remove_files, afd_fui,
			  remove_arch)) &&
      (remove_arch || remove_files || 
       (ok &= get_dir (newdir, &dir, fullpath)))) {
    if (remove_arch)
      rmarch (fullpath);
    else if (remove_files)
      rmfiles (optind, argc, argv, fullpath, no_compression, 0);
    else {
      if (tar)
	if ((ok &= tar_files (optind, argc, argv, tarf)))
	  argc = optind + 1;
	else 
	  goto abort;
      ok &= do_archive (optind, argc, argv, buf, fullpath, arch, dir, text,
			size, bin, uue, split, tar, no_compression, updatef);
      if (tar)
	unlink (tarf);
    }
  }
  abort:
  if(get_lock) lpl_unlock(LPL_GLOBAL_ARCHIVES,NULL);
  free ((char *) dir);
  free ((char *) buf);
  gexit (ok ? 0 : errno);
}

/*
  uuencode a file.
*/

void uuencode (FILE **fin, char *file, char *infile, struct stat *finbuf,
	       BOOLEAN tar)
{
  char *s;

  fclose (*fin);
  printf ("\t- uuencoding...\n");
  if (sysexec ("uuencode", NULL,
	       ((uue_file = lptmpnam(NULL)) ? uue_file : mystrdup ("")),
	       FALSE, NULL, FALSE, TRUE, file,
	       (s = tsprintf ("%s%s", infile, (tar ? ".tar" : ""))),
	       NULL))
    errno_tmp = errno,
    perror ("uuencode"),
    gexit (errno_tmp);
  free ((char *) s);
  *fin = fopen (uue_file, "r");
  stat (uue_file, finbuf);
}

/*
  tar all input files into 'tarf'.
*/

int tar_files (int optind, int argc, char **argv, char *tarf)
{
  char *_argv[1024];
  int original = optind, nargs = 0;

  if (touch (tarf)) {
    fprintf (stderr, "Cannot write to tar file %s\n", tarf);
    return 0;
  }
  unlink (tarf);
  printf ("tarring...\n");
  _argv[nargs++] = tarf;
  while (optind < argc) {
    if (access (argv[optind], R_OK)) {
      fprintf (stderr, "Unable to open %s for reading.\n", argv[optind]);
      return 0;
    }
    _argv[nargs++] = argv[optind++];
  }
  _argv[nargs] = NULL;
  if (sysexecv ("tar -cf", NULL, NULL, FALSE, NULL, FALSE, TRUE, _argv))
    errno_tmp = errno,
    perror ("tar"),
    gexit (errno_tmp);
  argv[original] = tarf;
  return 1;
}

/*
  Remove the file from the archive, if found.
*/

void rmfiles (int optind, int argc, char **argv, char *path,
	      BOOLEAN no_compression, BOOLEAN update_dirf_only)
{
  char dirf[1024];
  char file[1024];
  char fullpath[1024];
  char _fullpath[1024];
  char fullname[1024];
  char desc[1024];
  char *s, *tmpdir= NULL;
  BOOLEAN continued, found, match;
  int cnt, _cnt, i;
  long int count, sig_mask = 0;
  FILE *fin, *fout;
  struct stat sbuf;

  while (optind < argc) { /* Process each file */

    s = argv[optind] + strlen (argv[optind]) - 1;
    while (s != argv[optind] && *s == '/')  /* Remove trailing / */
      *(s--) = EOS;

    i = strlen (argv[optind]);
    if (i > 2 && argv[optind][i - 2] == '.' && argv[optind][i - 1] == 'Z')
      argv[optind][i - 2] = EOS;

    if (!update_dirf_only)
      printf ("%s: ", argv[optind]),
      fflush (stdout);
    RESET (dirf);
    sprintf (dirf, "%s/%s", path, DIRF);
    if (stat (dirf, &sbuf))
      errno_tmp = errno,
      printf ("not removed\n"),
      fprintf (stderr, "Cannot access index %s for reading.\n", dirf),
      errno = errno_tmp,
      perror ("stat()"),
      gexit (errno_tmp);
    BLOCK_SIGNAL (sig_mask, SIGINT);
    mv (dirf, ((tmpdir = lptmpnam(NULL)) ? tmpdir : mystrdup ("")));
    if ((fout = fopen (dirf, "w")) == NULL)
      errno_tmp = errno,
		perror ("fopen()"),
		fprintf (stderr, "Unable to open %s for writing.\nSEVERE ERROR: The \
archive is now in an inconsistent state with no DIR file.\nIts DIR file can be \
found in %s. Move %s to %s\n", dirf, tmpdir, tmpdir, dirf),
		free ((char *) tmpdir),
		gexit (errno_tmp);
    if ((fin = fopen (tmpdir, "r")) == NULL) { 
      errno_tmp = errno;
      perror ("fopen()");
      fprintf (stderr, "Unable to open %s for reading. Cannot proceed.\n",
			   tmpdir);
      if (stat (dirf, &sbuf))
		fputs ("SEVERE ERROR: The archive now has NO DIR file; DIR file \
lost!!!\n", stderr);
      else
		fputs ("Archive intact!\n", stderr);
      free ((char *) tmpdir);
      gexit (errno_tmp);
    }
    RESET (fullpath);
    found = FALSE;
    while (!feof (fin)) { /* Get location and file-count of file */
      file[0] = RESET (_fullpath);
      fscanf (fin, "%s %d ", file, &_cnt);
      match = FALSE;
      if (file[0] != EOS) {
		locase (file);
		if (!strcmp (argv[optind], file))
		  match = found = TRUE;
		if (!match)
		  fprintf (fout, "%s %d ", file, _cnt);
		for (i = 0; i < abs (_cnt); ++i) {
		  fscanf (fin, "%ld ", &count);
		  if (!match)
			fprintf (fout, "%ld ", count);
		}
		fscanf (fin, "%s", _fullpath);
		if (!match)
		  fprintf (fout, "%s", _fullpath);
		else {
		  cnt = _cnt;
		  strcpy (fullpath, _fullpath);
		  make_full_path(fullpath);
		}
		do { /* Get file description */
		  RESET (desc);
		  fgets (desc, MAX_LINE - 2, fin);
		  if (desc [0] != EOS && desc[LAST_CHAR (desc)] == '\n')
			desc[LAST_CHAR (desc)] = EOS;
		  continued = FALSE;
		  if (desc[0] != EOS && desc[LAST_CHAR (desc)] == '\\')
			desc[LAST_CHAR (desc)] = EOS,
			  continued = TRUE;
		  if (!match) {
			fprintf (fout, "%s", desc);
			if (continued)
			  fprintf (fout, "\\");
			fputs ("\n", fout);
		  }
		  else {
			if (!comment) {
			  if (!(comment = (char *) malloc ((strlen (desc) + 3) * sizeof (char))))
				fprintf (stderr, "\nrmfiles(): malloc() failed for %d bytes.\n",
						 (strlen (desc) + 3) * sizeof (char)),
				  gexit (EINVAL);
			  *comment = EOS;
			}
			else
			  if (! (comment = (char *) 
					 realloc ((char *) comment,
							  (strlen (comment) + strlen (desc) + 3) * sizeof (char))))
				fprintf (stderr, "\nrmfiles(): realloc() failed for %d bytes.\n",
						 (strlen (comment) + strlen (desc) + 3) * sizeof (char)),
				  gexit (EINVAL);
			strcat (comment, desc);
			if (continued)
			  strcat (comment, "\\\n");
		  }
		} while (continued);
		if (ferror (fout)) {
		  errno_tmp = errno;
		  perror ("fprintf()");
		  gexit (errno_tmp);
		}
      }
    }
    if (!found)
      printf ("not found in %s", dirf);
    else if (!update_dirf_only) { /* Remove all parts */
      printf ("removing parts: ");
      fflush (stdout);
      for (i = 1; i <= abs (cnt); i++) {
		RESET (fullname);
		if (abs (cnt) > 1)
		  sprintf (fullname, "%s/%s%d", fullpath, argv[optind], i);
		else
		  sprintf (fullname, "%s/%s", fullpath, argv[optind]);
		if (stat (fullname, &sbuf)) {
		  strcat (fullname, ".Z"); /* Check for compressed file */
		  if (no_compression || stat (fullname, &sbuf))
			printf ("%d (not found) ", i),
			  fflush (stdout);
		  else {
			printf ("%d ", i);
			if (unlink (fullname))
			  printf ("(errno %d, %s) ", errno, sys_errlist[errno]);
			fflush (stdout);
		  }
		}
		else {
		  printf ("%d ", i);
		  if (unlink (fullname))
			printf ("(errno %d, %s) ", errno, sys_errlist[errno]);
		  fflush (stdout);
		}
      }
    }
    else
      printf ("\t- updating...");
    fclose (fin);
    fclose (fout);
    RELEASE_SIGNAL (sig_mask, SIGINT);
    ++optind;
    puts ("");
    unlink (tmpdir);
  }
  if (tmpdir)
    free ((char *) tmpdir);
}

/*
  Remove the specified archive and all of its subarchives.
*/

void rmarch (char *fullpath)
{
  static char *archive_dir;
  char parent [MAX_LINE], *s;

  if (!archive_dir)
    archive_dir = ARCHIVE_DIR;
  /* First, un-archive all files from the archive and all files from its
     subarchives. */
  if (syscom ("\
arch_dir=%s/;\
dir=%s;\
index=%s;\
dirf=%s;\
farch=%s/farch;\
for path in `awk '{ print $2 }' $dir/$index`; do \
  arch=`echo $path | sed \"s:$arch_dir::g\"`;\
  echo Cleaning archive $arch:;\
  for file in `awk '{ print $1\" \" }' $path/$dirf`; do \
    $farch -K -a $arch -r $file;\
  done;\
done;\
", archive_dir, fullpath, INDEX, DIRF, install_dir()))
    errno_tmp = errno,
    fputs ("SEVERE ERROR: Could not un-archive existing files.\n", stderr),
    gexit (errno_tmp);

  /* Now, remove the archive and all of its subarchives from the top level
     INDEX and perhaps the parent (if any). */
  strcpy (parent, fullpath);
  if ((s = strrchr (parent, '/')))
    *s = EOS;
  if (syscom ("\
arch_dir=%s;\
def_arch=%s;\
index=%s;\
top_index=$arch_dir/$def_arch/$index;\
target_arch=%s;\
echo Removing the archive from $top_index...;\
egrep -v \" $target_arch \" $top_index > $top_index~;\
test -s $top_index~ && mv $top_index~ $top_index;\
parent=%s;\
if [ \"$parent\" != \"$arch_dir\" ]; then \
  top_index=$parent/$index;\
  echo Removing the archive from $top_index...;\
  egrep -v \" $target_arch \" $top_index > $top_index~;\
  test -s $top_index~ && mv $top_index~ $top_index;\
fi\
", archive_dir, DEFAULT_ARCHIVE, INDEX, fullpath, parent))
    errno_tmp = errno,
    fputs ("SEVERE ERROR: The system now has NO MASTER INDEX file; INDEX file \
lost!!!\n", stderr),
    gexit (errno_tmp);

/* Last, remove all physical directories. */
  if (syscom ("echo Removing all physical directories and files...;\
rm -rf %s", fullpath)) /* Remove all subarchive dirs */
    errno_tmp = errno,
    perror ("rm"),
    fputs (tsprintf ("SEVERE ERROR: cannot rm -rf %s\n", fullpath), stderr),
    gexit (errno_tmp);
}

/*
  Check for duplicate file name and return if so. Otherwise, update the
  archive's directory.
*/

int update (char *path, char *filename, long int count, long int *filesizes,
	    char *dir, char *text, struct stat finbuf, BOOLEAN pure_bin,
	    BOOLEAN no_compression, BOOLEAN updatef)
{
  char dirf[1024];
  char file[1024];
  char fullpath[1024];
  char desc[1024];
  char stripped_file[1024];
  char reply[80];
  BOOLEAN continued, found = FALSE;
  int cnt, i, sig_mask = 0;
  FILE *fout;
  char *reldir;
  

  /* construct an LPDIR-relative file name */
  /* reldir = make_relative_path(dir); */
  reldir = dir;


  strcpy (stripped_file, filename);
  i = strlen (stripped_file);
  if (i > 2 && stripped_file [i - 2] == '.' && stripped_file [i - 1] == 'Z')
    stripped_file [i - 2] = EOS;
  RESET (dirf);
  sprintf (dirf, "%s/%s", path, DIRF);
  if ((fout = fopen (dirf, "r")) == NULL) {
    errno_tmp = errno;
	perror ("fopen()");
	fprintf (stderr, "Unable to open index %s for reading.\nFile %s \
not archived. Remove output file(s) from %s\n", dirf, filename, dir);
	gexit (errno_tmp);
  }
  while (!feof (fout)) { /* Get location and file-count of file */
    file[0] = RESET (fullpath);
    fscanf (fout, "%s %d %s", file, &cnt, fullpath);
    if (file[0] != EOS) {
      do { /* Get file description */
		RESET (desc);
		fgets (desc, MAX_LINE - 2, fout);
		if (desc [0] != EOS && desc[LAST_CHAR (desc)] == '\n')
		  desc[LAST_CHAR (desc)] = EOS;
		continued = FALSE;
		if (desc[0] != EOS && desc[LAST_CHAR (desc)] == '\\')
		  desc[LAST_CHAR (desc)] = EOS,
			continued = TRUE;
      } while (continued);
      locase (file);
      if (!strcmp (stripped_file, file)) {
        found = TRUE;
        break;
      }
    }
  }
  fclose (fout);
  if (found) {
    if (updatef)
      rmfiles (0, 1, &filename, path, no_compression, 1);
    else {
      fprintf (stderr, "\t- file %s already exists in %s\n\
\t- file not archived\n", stripped_file, dirf);
      remove_files (dir, filename, count, finbuf, no_compression);
      return 0;
    }
  }
  else if (updatef)
    fprintf (stderr, "WARNING: %s is not currently archived in %s\n",
			 stripped_file, dirf);
  BLOCK_SIGNAL (sig_mask, SIGINT);
  if ((fout = fopen (dirf, "a")) == NULL)
    errno_tmp = errno,
	  perror ("fopen()"),
	  fprintf (stderr, "Unable to open index %s for update.\nFile %s \
not archived. Exiting.\n", dirf, filename),
	  remove_files (dir, filename, count, finbuf, no_compression),
	  gexit (errno_tmp);
  fprintf (fout, "%s %ld ", stripped_file, (pure_bin ? -count : count));
  for (i = 0; i < count; i++)
    fprintf (fout, "%ld ", filesizes[i]);
  fprintf (fout, "%s", reldir);
  if (updatef && comment && text[0] == EOS) {
    fprintf (fout, "%s\n", comment);
    free ((char *) comment);
    comment = NULL;
  }
  else
    fprintf (fout, "%s%s\n", (text[0] != EOS && strcmp (text, " ") ? " " : ""),
			 (strcmp (text, " ") ? text : ""));
  if (ferror (fout))
    errno_tmp = errno,
	  perror ("fprintf()"),
	  gexit (errno_tmp);
  fclose (fout);
  RELEASE_SIGNAL (sig_mask, SIGINT);
  return 1;
}

/*
  Remove all parts of the specified file from the specified directory.
*/

void remove_files (char *dir, char *filename, long int count,
		   struct stat finbuf, BOOLEAN no_compression)
{
  char file[1024];
  char fout[1024], *s;
  struct stat foutbuf;
  char yn;
  BOOLEAN split = (count > 1 ? 1 : 0);

  if (!split) {
    sprintf (fout, "%s/%s", dir, filename);
    if (!no_compression)
      sysexec ("uncompress", "/dev/null", "/dev/null", FALSE, STDERR_TO_STDOUT,
	       FALSE, TRUE, (s = tsprintf ("%s.Z", fout)), NULL),
      free ((char *) s);
    if (stat (fout, &foutbuf)) {
      printf ("Input file %s may be the same as output file %s/%s.\n\
Remove %s/%s [y]? ",
	      filename, dir, filename, dir, filename);
      fflush (stdout);
      scanf ("%c", &yn);
      if (yn == 'n' || yn == 'N')
        return;
    }
    if ((finbuf.st_ino == foutbuf.st_ino) &&
	(finbuf.st_dev == foutbuf.st_dev)) {
      printf ("\t- output file matches input: not removed\n");
      return;
    }
  }
  while (count) {
    RESET (file);
    if (split)
      sprintf (file, "%s/%s%d", dir, filename, count);
    else
      sprintf (file, "%s/%s", dir, filename);
    unlink (file);
    if (!no_compression)
      strcat (file, ".Z"),
      unlink (file);
    --count;
  }
}

void usage (char *s, int exitval)
{
  fprintf (stderr, "%s {{[-n] [-b | -B] [-s size] \
[-d dir] [-p password] [-D 'description'] [-t file] [-u]} | {<-r>} \
[<-a | -A> archive | path-to-archive] [-Z] [file(s)]} | {<-R archive>}\n", s);
  fprintf (stderr, "\
-n: do not split input file(s)\n\
-b: input files are binary: uuencode (on if -t specified)\n\
-B input files are binary: do not uuencode and do not split\n\
-s: maximum size of each output file (in kilobytes) -- default %ld\n\
-d: actual directory to put output file(s)\n    -- default as specified by -a: %s/<archive>\n\
-p: password for a new private archive\n\
-D: description of the file to go into the DIR file\n\
-t: tar all input files into 'file' and archive that\n\
-u: update existing files\n\
-a: which archive index to update (subdir of %s)\n    -- \
default %s\n\
-A: same as -a, but mark the archive for AFD/FUI usage\n\
-r: remove the specified file(s) from the archive\n\
-Z: turn off file compression\n\
-R: remove the specified archive and all of its subarchives\n",
(long) DEFAULT_SIZE / 1024, ARCHIVE_DIR, ARCHIVE_DIR, DEFAULT_ARCHIVE);
  exit (exitval);
}

/*
  Archive all files given in 'argv'.
*/

int do_archive (int optind, int argc, char **argv, char *buf, char *fullpath,
	        char *archive, char *dir, char *text, long int size,
		BOOLEAN bin, BOOLEAN uue, BOOLEAN split, BOOLEAN tar,
		BOOLEAN no_compression, BOOLEAN updatef)
{
  FILE *fin, *fout, *flast;
  long int l, count, nread, nchars, total, *filesizes = NULL, ok = 1;
  char *leftovers = NULL, *s, *infile = NULL, filename[1024],
    compressed[1024];
  struct stat sbuf, finbuf, foutbuf;

  while (optind < argc) { /* Split and archive each file */

    s = argv[optind] + strlen (argv[optind]) - 1;
    while (s != argv[optind] && *s == '/')  /* Remove trailing / */
      *(s--) = EOS;

    if ((fin = fopen (argv[optind], "r")) == NULL) { /* Open file */
      fprintf (stderr, "Cannot open file %s for reading.\n", argv[optind]);
      ok = 0;
      goto abort;
    }

    printf ("%s:\n", argv[optind]);
    count = 1;
    nchars = 0;

    if (infile) /* free previous allocation */
      free ((char *) infile);
    infile = extract_filename (argv[optind]); /* get filename from path */
    locase (infile);
    l = strlen (infile);
    if (l > 2 && infile [l - 2] == '.' && infile [l - 1] == 'z') {
      infile [l - 2] = EOS;
      l = strlen (argv[optind]);
      argv[optind][l - 2] = EOS;
      printf ("\t- uncompressing...\n");
      sysexec ("uncompress", "/dev/null", "/dev/null", FALSE, STDERR_TO_STDOUT,
	       FALSE, TRUE, argv[optind], NULL);
      fclose (fin);
      fin = fopen (argv[optind], "r");
    }

    stat (argv[optind], &finbuf); /* Get size of input file */
    if (bin && uue) /* uuencode */
      uuencode (&fin, argv[optind], infile, &finbuf, tar);

    if (!split || finbuf.st_size <= size) { /* Not splitting;may have to copy */
      fclose (fin);
      sprintf (filename, "%s%s%s", (strcmp (dir, "/") ? dir : ""),
               ((strcmp (dir, infile) && strcmp (infile, "/")) ? "/" : ""),
               infile);

      /* The following could potentially remove a file with the same name
	 that does not belong to the archived file; should probably ask
	 for permission; then the ARCHIVE request will have to be modified
	 so that a YES should be sent */
      if (updatef && !stat (filename, &foutbuf) &&
	  finbuf.st_ino != foutbuf.st_ino)	/* Remove old file */
	unlink (filename);

      if (bin) { /* Copy (possibly) uue_file to target directory */
        if (uue && !stat (filename, &sbuf)) {
	  fprintf (stderr, "Output file %s exists; cannot proceed with %s\n",
		   filename, argv[optind]);
	  unlink (uue_file);
	  free ((char *) uue_file);
	  ok = 0;
	  goto abort;
	}
        if (uue && (fout = fopen (filename, "w")) == NULL) { /* Test */
          fprintf (stderr, "Cannot write to directory %s; cannot \
proceed with %s\n",
                   dir, argv[optind]);
	  unlink (uue_file);
	  free ((char *) uue_file);
	  ok = 0;
          goto abort;
        }
	if (uue) {
          fclose (fout);
          if (mv (uue_file, filename))
	    errno_tmp = errno,
	    perror ("mv()"),
	    gexit (errno_tmp);
	}
	else if (stat (filename, &sbuf)) { /* File does not exist in target dir */
	  if ((fout = fopen (filename, "w")) == NULL) { /* Test */
	    fprintf (stderr, "Cannot write to directory %s; cannot \
proceed with %s\n",
		     dir, argv[optind]);
	    ok = 0;
	    goto abort;
	  }
	  fclose (fout);
	  if (cp (argv[optind], filename))
	    errno_tmp = errno,
	    perror ("cp()"),
	    gexit (errno_tmp);
	}
	else if (!stat (filename, &foutbuf) &&
		 finbuf.st_ino != foutbuf.st_ino) {
	  fprintf (stderr, "Output file %s exists; cannot proceed with %s\n",
		   filename, argv[optind]);
	  ok = 0;
	  goto abort;
	}
      }
      else if (stat (filename, &sbuf)) { /* File does not exist in target dir */
        if ((fout = fopen (filename, "w")) == NULL) {
          fprintf (stderr, "Cannot write to directory %s; cannot \
proceed with %s\n",
                   dir, argv[optind]);
	  ok = 0;
          goto abort;
        }
        fclose (fout);
        if (cp (argv[optind], filename))
	  errno_tmp = errno,
	  perror ("cp()"),
	  gexit (errno_tmp);
      }
      else if (!stat (filename, &foutbuf) &&
	       finbuf.st_ino != foutbuf.st_ino) {
	fprintf (stderr, "Output file %s exists; cannot proceed with %s\n",
		 filename, argv[optind]);
	ok = 0;
	goto abort;
      }

      if (!no_compression) {
	sprintf (compressed, "%s.Z", filename);
	/* The following could potentially remove a file with the same name
	   that does not belong to the archived file; should probably ask
	   for permission; then the ARCHIVE request will have to be modified
	   so that a YES should be sent */
	if (updatef)
	  unlink (compressed);
	else if (!stat (compressed, &sbuf)) {
	  fprintf (stderr, "Output file %s exists; cannot proceed with %s\n",
		   compressed, argv[optind]);
	  if (bin && uue)
	    unlink (uue_file),
	    free ((char *) uue_file);
	  ok = 0;
	  goto abort;
	}
      }

      if (!(filesizes = (long int *) malloc (sizeof (long int))))
	fprintf (stderr, "\ndo_archive(): malloc() failed for %d bytes.\n",
		 sizeof (long int)),
	gexit (EINVAL);
      *filesizes = (long int) finbuf.st_size;
      lpfile_chmod(filename,0666);
      if (!no_compression)
	sysexec ("compress", "/dev/null", "/dev/null", FALSE, STDERR_TO_STDOUT,
		 FALSE, TRUE, filename, NULL);
      if (update (fullpath, infile, 1, filesizes, dir, text, finbuf,bin & !uue,
		  no_compression, updatef))
        printf ("\t- file not split, %s\n\t- archived in %s\n\t- directory: %s\n",
                (no_compression ? "not compressed" : "compressed"),
		archive, dir);
      free ((long int *) filesizes);
      filesizes = NULL;
    }
    else { /* Split file */
      while (!feof (fin)) {
        sprintf (filename, "%s%s%s%d", (strcmp (dir, "/") ? dir : ""),
                 ((strcmp (dir, infile) && strcmp (infile, "/")) ? "/" : ""),
                 infile, count);

	/* The following could potentially remove a file with the same name
	   that does not belong to the archived file; should probably ask
	   for permission; then the ARCHIVE request will have to be modified
	   so that a YES should be sent; no need to check i-node because
	   it will be different from finbuf's because of the extra character
	   at the end */
	if (updatef)
	  unlink (filename);
        else if (!stat (filename, &sbuf)) {
          fprintf (stderr, "Output file %s exists; cannot proceed with %s\n",
                   filename, argv[optind]);
	  if (bin)
	    unlink (uue_file),
	    free ((char *) uue_file);
          fout = (FILE *) -1;
          break;
        }
        if ((fout = fopen (filename, "w")) == NULL) {
          fprintf (stderr, "Cannot write to directory %s; cannot proceed \
with %s\n",
                   dir, argv[optind]);
          fout = (FILE *) -1;
          break;
        }
		lpfile_chmod(filename,0666);

        total = 0;  /* Do the actual splitting */
        total += fwrite (leftovers, sizeof (char), nchars, fout);
	if (ferror (fout))
	  errno_tmp = errno,
	  perror ("fwrite()"),
	  gexit (errno_tmp);
        nread = fread (buf, sizeof (char), size - nchars, fin);
        if (nchars)
          free ((char *) leftovers);
        s = buf + nread - (nread > 0 ? 1 : 0);
        nchars = 0;
        while (s != buf && *s != '\n') /* Look for first \n from the end */
          ++nchars,
          --s;
        if (s == buf) /* No new line found; write as is */
          nchars = 0;
        nread -= nchars;
	if (nchars) {
	  if (!(leftovers = (char *) malloc (nchars * sizeof (char))))
	    fprintf (stderr, "\ndo_archive: malloc() failed for %d bytes.\n",
		     nchars * sizeof (char)),
	    gexit (EINVAL);
	  strncpy (leftovers, ++s, nchars); /* Copy anything after \n */
	}
        total += fwrite (buf, sizeof (char), nread, fout);
	if (ferror (fout))
	  errno_tmp = errno,
	  perror ("fwrite()"),
	  gexit (errno_tmp);
        fclose (fout);

        if (total == 0)
          unlink (filename);
        else { /* OK, compress */
	  if (!no_compression) {
	    sprintf (compressed, "%s.Z", filename);
	    /* The following could potentially remove a file with the same name
	       that does not belong to the archived file; should probably ask
	       for permission; then the ARCHIVE request will have to be modified
	       so that a YES should be sent; no need to check i-node because
	       it will be different from finbuf's because of the extra character
	       at the end */
	    if (updatef)
	      unlink (compressed);
	    else if (!stat (compressed, &sbuf)) {
	      fprintf (stderr, "Output file %s exists; cannot proceed with %s\n",
		       compressed, argv[optind]);
	      if (bin)
		unlink (uue_file),
		free ((char *) uue_file);
	      unlink (filename);
	      ok = 0;
	      goto abort;
	    }
	  }
	  stat (filename, &sbuf);
	  if (!(filesizes = (long int *)
		(filesizes ?
		 (realloc ((long int *) filesizes, count * sizeof (long int))) :
		 (malloc (count * sizeof (long int))))))
	    fprintf (stderr, "\ndo_archive(): malloc()/realloc() failed for %d bytes.\n",
		     count * sizeof (long int)),
	    gexit (EINVAL);
	  *(filesizes + (count - 1)) = (long int) sbuf.st_size;
	  if (!no_compression)
	    sysexec ("compress", "/dev/null", "/dev/null", FALSE, 
		     STDERR_TO_STDOUT, FALSE, TRUE, filename, NULL);
	  ++count; /* Always one ahead */
	}
      }

      fclose (fin);
      if (bin)
        unlink (uue_file),
	free ((char *) uue_file);
      if (nchars) { /* write any leftovers */
        flast = fopen (filename, "a");
        fwrite (leftovers, sizeof (char), nchars, flast);
	if (ferror (fout))
	  errno_tmp = errno,
	  perror ("fwrite()"),
	  gexit (errno_tmp);
        fclose (flast);
        free ((char *) leftovers);
	stat (filename, &sbuf);
	*(filesizes + (count - 2)) = (long int) sbuf.st_size;
      }

      if (fout != (FILE *) -1) { /* Splitting successful, update DIR file */
        if (update (fullpath, infile, count - 1, filesizes, dir, text, finbuf,
	    bin & !uue, no_compression, updatef))
          printf ("\t- file split in %ld %scompressed parts\n\t- archived in %s\n\t\
- directory: %s\n",
		  (long) count - 1, 
                  (no_compression ? "un" : ""), 
		  archive, dir);
	free ((long int *) filesizes);
	filesizes = NULL;
      }
    }
  abort:
    ++optind;
  }
  return ok;
}

/*
  Get archive to update.
*/

int get_archive (char **archive, char *fullpath, char *password,
		 BOOLEAN remove_file, BOOLEAN afd_fui, BOOLEAN remove_arch)
{
  FILE *master;
  char indexf[1024], line[1024], cur_archive[1024], arch[1024], dirf[1024],
  cur_indexf [1024], error [1024], *slash;
  char original[1024];
  BOOLEAN found = FALSE;
  static char *archive_dir;
  BOOLEAN prev_afd_fui = FALSE;

  if (!*archive)
    *archive = DEFAULT_ARCHIVE;
  if (!archive_dir)
    archive_dir = ARCHIVE_DIR;
  strcpy (cur_archive, (char *) *archive);
  strcpy (original, *archive);
  if ((slash = strchr (cur_archive, '/')))
    *slash = EOS;
  sprintf (indexf, "%s/%s/%s", archive_dir, DEFAULT_ARCHIVE, INDEX);
  while (*archive[0] != EOS) { /* Check all archives specified */
    if ((master = fopen (indexf, "r")) == NULL) {
      fprintf (stderr, "Sorry, no master index found in %s.\n", archive_dir);
      return 0;
    }
    found = FALSE;
    strcpy (cur_indexf, indexf);
    while (!feof (master)) { /* Look at the current index for fullpath */
      indexf[0] = arch[0] = RESET (line);
      fgets (line, MAX_LINE - 2, master);
      if (line[0] != EOS) {
        sscanf (line, "%s %s\n", arch, indexf);
        locase (arch);
        if (!strcmp (&arch[(arch[0] == AFD_FUI_CHAR ? 1 : 0)], cur_archive)) {
	  prev_afd_fui = (arch[0] == AFD_FUI_CHAR ? 1 : 0);
          found = TRUE;
          break;
        }
      }
    }
    fclose (master);
    if (!found && (remove_file || remove_arch)) {
      fprintf (stderr, "%s: not a valid archive or path.\n", cur_archive);
      return 0;
    }
    sprintf (dirf, "%s/%s/%s", archive_dir, original, DIRF);
    if (!found && !remove_arch) { 
      printf ("New archive %s:\n", original);
      if (!make_indexes (dirf, original, password, error, 
						 lpfile_ulistproc_umask, 
						 lpfile_ulistproc_archives_umask, afd_fui)) {
		fprintf (stderr, "%s", error);
		return 0;
      }
      sprintf (indexf, "%s/%s", archive_dir, original);
    }
    if ((slash = strchr (*archive, '/')))
      sprintf (*archive, "%s", slash + 1); /* Move down the path */
    else
      sprintf (*archive, "%s", *archive + strlen (cur_archive));
    strcpy (cur_archive, *archive);
    if ((slash = strchr (cur_archive, '/')))
      *slash = EOS;
    if (cur_archive[0] != EOS)
      sprintf (indexf, "%s/%s", indexf, INDEX);
  }
  if (!remove_arch && prev_afd_fui != afd_fui) {
    char *sedcmd;
    if (afd_fui)
      sedcmd = tsprintf ("s:^$target_arch[ \t][ \t]*$dir:\
%c$target_arch $dir:g", AFD_FUI_CHAR);
    else
      sedcmd = tsprintf ("s:^%c$target_arch[ \t][ \t]*$dir:\
$target_arch $dir:g", AFD_FUI_CHAR);
    if (syscom ("\
def_arch=%s;\
index=%s;\
arg=`echo %s | sed \"s:$def_arch/::g\"`;\
dir=%s;\
arch_dir=%s;\
top_arch=$arch_dir/$def_arch;\
target_arch=`basename $arg`;\
sedcmd=\"%s\";\
while [ $dir != $arch_dir ]; do\
  tr '[A-Z]' '[a-z]' < $dir/$index | sed \"$sedcmd\" > $dir/$index~;\
  test -s $dir/$index~ && mv $dir/$index~ $dir/$index;\
  dir=`dirname $dir`;\
done;\
sed \"$sedcmd\" $arch_dir/$def_arch/$index > $arch_dir/$def_arch/$index~;\
test -s $arch_dir/$def_arch/$index~ && mv $arch_dir/$def_arch/$index~ $arch_dir/$def_arch/$index", 
		DEFAULT_ARCHIVE, INDEX, original, indexf, archive_dir, sedcmd))
      errno_tmp = errno,
      fputs ("SEVERE ERROR: The archive now has NO INDEX file; INDEX file \
lost!!!\n", stderr),
      gexit (errno_tmp);
    free ((char *) sedcmd);
    printf ("\t- turned afd/fui for archive %s %s\n", original, 
	    (afd_fui ? "on" : "off"));
  }
  strcpy (fullpath, indexf);
  strcpy (*archive, original);
  return 1;
}

/*
  Get output directory.
*/

int get_dir (char *newdir, char **dir, char *fullpath)
{
  char *s;
  char msg [MAX_LINE];
  struct stat sbuf;

  free ((char *) *dir);
  if (!newdir) {
    if (!(*dir = (char *) malloc ((strlen (fullpath) + 1) * sizeof (char))))
      fprintf (stderr, "\nget_dir(): malloc() failed for %d bytes.\n",
	       (strlen (fullpath) + 1) * sizeof (char)),
      gexit (EINVAL);
    sprintf (*dir, "%s", fullpath);
  }
  else {
    if (!(*dir = (char *) malloc ((strlen (newdir) + 1) * sizeof (char))))
      fprintf (stderr, "\nget_dir(): malloc() failed for %d bytes.\n",
	       (strlen (newdir) + 1) * sizeof (char)),
      gexit (EINVAL);
    sprintf (*dir, "%s", newdir);
    s = *dir + strlen (*dir) - 1;
    while (s != *dir && *s == '/')
      *(s--) = EOS;
  }
  if (stat (*dir, &sbuf)) {
    if (!mkdir1 (*dir, msg, lpfile_ulistproc_archives_umask)) {
      fprintf (stderr, "Unable to create new directory %s\n%s", *dir, msg);
      return 0;
    }
    printf ("Creating new directory %s\n", *dir);
  }
  return 1;
}

/*
  Graceful exit.
*/

int gexit (int exitcode)
{
  exit (exitcode);
}
