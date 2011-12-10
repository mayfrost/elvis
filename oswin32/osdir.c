/* oswin32/osdir.c */

/* This file contains functions which deal with filenames.  The structure of
 * this file is a little unusual because some of the support programs also
 * use functions from this file.  To minimize the size of those support
 * programs, this file may be compiled with JUST_DIRFIRST or JUST_DIRPATH
 * defined to exclude the unused functions.  If neither of those names is
 * defined, then the whole file is compiled.
 */

#if !defined(JUST_DIRFIRST) && !defined(JUST_DIRPATH)
# include "elvis.h"
#endif
#include <stdlib.h>
#include <io.h>
#include <direct.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifndef MAX_PATH
# define MAX_PATH (_MAX_FNAME * 3)
#endif

#if defined(JUST_DIRFIRST) || defined(JUST_DIRPATH)
# undef isalpha
# define isalpha(c)	((unsigned)(((c) & 0xffdf) - 'A') < 26)
#endif /* JUST_DIRFIRST */

#ifndef JUST_DIRPATH

/* These are used for communication between dirfirst() and dirnext() */
static char			finddir[MAX_PATH];
static char			findwild[MAX_PATH];
static char			found[MAX_PATH];
#ifdef WIN32
static struct _finddata_t	FileData;
#else 
static struct _find_t		FileData;
#endif
static long			hSearch;

/* Return the first filename (in a static buffer) that matches
 * wildexpr, or wildexpr if none matches.  If wildexpr has no contains
 * no wildcards, then just return wildexpr without checking for files.
 */
char *dirfirst(char *wildexpr, BOOLEAN ispartial)
{
	/* remember the directory name */
	strcpy(finddir, dirdir(wildexpr));

	/* Copy the wildexpr into fildwild[].  If it is meant to be a partial
	 * name, then append "*" to it.
	 */
	strcpy(findwild, wildexpr);
	if (ispartial)
		strcat(findwild, "*");

	/* if no match, then return the original wildexpr unchanged */
	hSearch = _findfirst(findwild, &FileData);
	while (hSearch >= 0 && *FileData.name == '.')
	{
		if (_findnext(hSearch, &FileData) < 0)
		{
			_findclose(hSearch);
			hSearch = -1;
		}
	}
	if (hSearch < 0)
	{
		found[0] = '\0';
		return wildexpr;
	}

	/* combine the directory name with the found file's name */
	strcpy(found, dirpath(finddir, FileData.name));

	return found;
}

/* Return the next filename (in a static buffer) that matches the
 * wildexpr of the previous dirfirst(), or NULL if no more files match.
 */
char *dirnext(void)
{
	/* if previous call returned NULL, then return NULL again. */
	if (!found[0])
		return NULL;

	/* if there is no match, then return NULL */
	do
	{
		if (_findnext(hSearch, &FileData) < 0)
		{
			found[0] = '\0';
			_findclose(hSearch);
			return NULL;
		}
	} while (*FileData.name == '.');

	/* combine the directory name with the found file's name */
	strcpy(found, dirpath(finddir, FileData.name));

	return found;
}

#ifndef JUST_DIRFIRST

/* Return True if wildexpr contains any wildcards; else False */
BOOLEAN diriswild(char *wildexpr)
{
	if (strchr(wildexpr, '*') || strchr(wildexpr, '?'))
	{
		return True;
	}
	return False;
}


/* Check a the type & permissions of a file.  Return one of the
 * following to describe the file's type & permissions:
 *    DIR_INVALID    malformed filename (can't happen with UNIX)
 *    DIR_BADPATH    unable to check file
 *    DIR_NOTFILE    directory or other non-file
 *    DIR_NEW        file doesn't exist yet
 *    DIR_UNREADABLE file exists but is unreadable
 *    DIR_READONLY   file is readable but not writable
 *    DIR_READWRITE  file is readable and writable.
 */
DIRPERM dirperm(char *filename)
{
	struct _stat statb;
	int	i;

	/* check for a protocol */
	for (i = 0; isalpha(filename[i]); i++)
	{
	}
	if (i < 2 || filename[i] != ':')
		i = 0;

	/* skip past "file:" protocol; assume all others are readonly */
	if (!strncmp(filename, "file:", 5))
		filename += i + 1;
	else if (!strncmp(filename, "ftp:", 4))
		return DIR_READWRITE;
	else if (i > 0)
		return DIR_READONLY;

	if (_stat(filename, &statb) < 0)
	{
		if (errno == ENOENT)
			return DIR_NEW;
		else
			return DIR_BADPATH;
	}
	if ((statb.st_mode & _S_IFMT) != _S_IFREG
	 || !strcmp(filename, "nul")
	 || !strcmp(filename, "prn"))
		return DIR_NOTFILE;
	if ((statb.st_mode & _S_IWRITE) == 0)
		return DIR_READONLY;
	return DIR_READWRITE;
}
#endif /* !JUST_DIRFIRST */
#endif /* !JUST_DIRPATH */

/* return the directory part of a pathname */
char *dirdir(char *pathname)
{
 static char	ret[MAX_PATH];
	char	*slash;

	/* break the name at the last backslash */
	strcpy(ret, pathname);
	for (slash = &ret[strlen(ret)];
	     --slash >= ret && *slash != '/' && *slash != '\\';
	     )
	{
	}
	if (slash == ret)
	{
		ret[1] = '\0';
	}
	else if (isalpha(ret[0]) && ret[1] == ':' && slash == &ret[2])
	{
		ret[3] = '\0';
	}
	else if (slash >= ret)
	{
		*slash = '\0';
	}
	else if (isalpha(ret[0]) && ret[1] == ':')
	{
		ret[2] = '\0';
	}
	else
	{
		strcpy(ret, ".");
	}
	return ret;
}

#ifndef JUST_DIRPATH
/* return the file part of a pathname.  This particular implementation doesn't
 * use an internal buffer; it simply returns a pointer to the filename at the
 * end of the pathname.
 */
char *dirfile(char *pathname)
{
	char	*backslash;
	char	*slash;

	backslash = strrchr(pathname, '\\');
	slash = strchr(pathname, '/');
	if (!slash || (backslash && backslash > slash))
		slash = backslash;
	if (!slash)
	{
		slash = strrchr(pathname, ':');
	}
	if (!slash)
	{
		slash = pathname - 1;
	}
	return slash + 1;
}
#endif /* !JUST_DIRPATH */


/* combine a directory name and a filename, yielding a pathname. */
char *dirpath(char *dir, char *file)
{
	static char	path[MAX_PATH + 1];
	char		*build = path;

	/* choose a drive letter, from either dir or file.  If file is used,
	 * then ignore dir completely.
	 */
	if (isalpha(file[0]) && file[1] == ':')
	{
		*build++ = *file++;
		*build++ = *file++;
		dir = ".";
	}
	else if (isalpha(dir[0]) && dir[1] == ':')
	{
		*build++ = *dir++;
		*build++ = *dir++;
	}

	/* if file has absolute path, or dir is ".", then ignore dir.  Else
	 * add dir to the path now, deleting the terminal backslash (if any).
	 */
	if (file[0] != '/' && file[0] != '\\' && strcmp(dir, "."))
	{
		while (*dir)
		{
#if 0
			switch (*dir)
			{
			  case '/':	if (dir[1]) *build++ = '\\';	break;
			  case '\\':	if (dir[1]) *build++ = *dir;	break;
			  default:	*build++ = *dir;		break;
			}
#else
			if (*dir == '/')
				*build++ = '\\';
			else
				*build++ = *dir;
#endif
			dir++;
		}
	}

	/* add a backslash, if necessary. */
	if (build != path && build[-1] != ':' && build[-1] != '\\' && *file != '/' && *file != '\\')
		*build++ = '\\';

	/* add the filename */
	while (*file)
	{
		if (*file == '/')
			*build++ = '\\', file++;
		else
			*build++ = *file++;
	}
	*build = '\0';
	return path;
}

#ifndef JUST_DIRFIRST
#ifndef JUST_DIRPATH

/* return the pathname of the current working directory */
char *dircwd(void)
{
	static char	cwd[MAX_PATH + 1];

	if (_getcwd(cwd, sizeof cwd))
	{
		return cwd;
	}
	else
	{
		return ".";
	}
}


/* Change working directory and return True if successful */
BOOLEAN dirchdir(pathname)
	char	*pathname;	/* new directory name */
{
	return (BOOLEAN)(_chdir(pathname) >= 0);
}


/* OS-dependent initialization function.  Here we initialize the o_elvispath
 * option to include the executables directory among the possible library
 * directories.
 */
void osinit(argv0)
	char	*argv0;
{
	char	*tmp;
 static	char	path[260];

	/* if argv0 isn't a pathname, then try to locate "elvis.exe" in
	 * the execution path.  We need this when figuring out the default
	 * value for the "elvispath" option and maybe the "home" option.
	 */
	if (!strchr(argv0, '\\') && _access("elvis.exe", 0) != 0)
	{
		tmp = getenv("PATH");
		if (tmp)
		{
			tmp = iopath(tmp, "elvis.exe", False);
			if (tmp)
			{
				argv0 = tmp;
			}
		}
	}

	/* Set the "home" option.  This is set (by order of preference) to
	 * one of the following:
	 *	- the value of the HOME environment variable, if set
	 *	- the values of HOMEDRIVE and HOMEPATH, if set
	 *	- the pathname of the elvis.exe program, if known
	 *	- C:\
	 *
	 * NOTE: The portable code in "optglob.c" will override this value
	 * if the HOME environment variable is set.
	 */
	if (getenv("HOMEDRIVE") && getenv("HOMEPATH"))
	{
		/* derive from HOMEDRIVE and HOMEPATH */
		optpreset(o_home, (CHAR *)safealloc(strlen(getenv("HOMEPATH")) + 3, sizeof(CHAR)), OPT_FREE);
		sprintf(tochar8(o_home), "%.2s%s", getenv("HOMEDRIVE"), getenv("HOMEPATH"));
	}
	else if (strchr(argv0, '\\'))
	{
		/* program name has a path.  Use it as home */
		optpreset(o_home, CHARdup(toCHAR(dirdir(argv0))), OPT_FREE);
	}
	else
	{
		/* last resort -- just use C:\ */
		o_home = toCHAR("C:\\");
	}

	/* Set the "elvispath" option.  We want this to include ~\elvislib,
	 * plus the directory where ELVIS.EXE resides and the lib subdirectory
	 * under that directory.
	 *
	 * NOTE: The portable code in "optglob.c" will override this value
	 * if the ELVISPATH environment variable is set.
	 */
	tmp = dirdir(argv0);
	sprintf(path, "~\\elvislib;%s;%s", tmp, dirpath(tmp, "lib"));
	o_elvispath = toCHAR(path);
}

#endif /* !JUST_DIRPATH */
#endif /* !JUST_DIRFIRST */