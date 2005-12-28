/* 
 * Copyright (c) 2005, 2006 Miek Gieben
 * See LICENSE for the license
 */

#define _GNU_SOURCE
#include "rdup.h"

/* options */
int opt_null = 0;
int opt_onefilesystem = 0;
int opt_nobackup = 1;
int opt_verbose = 0;

int dumptype;
time_t list_mtime;

/* crawler.c */
gboolean dir_crawl(GTree *t, char *path);
gboolean dir_prepend(GTree *t, char *path);

void
usage(FILE *f) 
{	
	fprintf(f, "Usage: %s [OPTION...] FILELIST DIR...\n", PROGNAME);
	fprintf(f, "%s generates a full or incremental file list, this\n", PROGNAME);
	fprintf(f, "list can be used to implement a (incremental) backup scheme\n");
	fprintf(f, "\n   FILELIST\tincremental file list\n");
	fprintf(f, "   \t\tif not found or empty, a full dump is done\n");
	fprintf(f, "   DIR\t\tdirectory or directories to dump\n");
	fprintf(f, "\nOptions:\n");
	fprintf(f, "   -h\t\tgives this help\n");
	fprintf(f, "   -V\t\tprint version\n");
	fprintf(f, "   -n\t\tdo not look at" NOBACKUP "files\n");
	fprintf(f, "   -v\t\tbe more verbose\n");
	fprintf(f, "   -x\t\tstay in local file system\n");
	fprintf(f, "   -0\t\tdelimit all output with NULLs\n");
	fprintf(f, "\nReport bugs to <miek@miek.nl>\n");
	fprintf(f, "Licensed under the GPL. See the file LICENSE in the\n");
	fprintf(f, "source distribution of rdup.\n");

}

void
version(FILE *f) 
{	
	fprintf(f, "%s %s\n", PROGNAME, VERSION);
}

/**
 * subtrace tree *b from tree *a, leaving
 * the elements that are only in *a. Essentially
 * a double diff: A diff (A diff B)
 */
GTree *
g_tree_substract(GTree *a, GTree *b)
{
	GTree 	         *diff;
	struct substract s;

	diff = g_tree_new(gfunc_equal);
	s.d = diff;
	s.b = b;
	/* everything in a, but NOT in b 
	 * diff gets filled inside this function */
	g_tree_foreach(a, gfunc_substract, (gpointer)&s);
	return diff;
}

/** 
 * read a filelist, which should hold our previous
 * backup list
 */
GTree *
g_tree_read_file(FILE *fp)
{
	char 	      *buf;
	char          *n;
	char 	      delim;
	mode_t        modus;
	GTree         *tree;
	struct entry *e;
	size_t        s;

	tree = g_tree_new(gfunc_equal);
	buf  = g_malloc(BUFSIZE);
	s    = BUFSIZE;

	if (opt_null) {
		delim = '\0';
	} else {
		delim = '\n';
	}

	while ((getdelim(&buf, &s, delim, fp)) != -1) {
		if (s < LIST_MINSIZE) {
			fprintf(stderr, "** Corrupt entry in filelist\n");
			continue;
		}
		if (!opt_null) {
			n = strrchr(buf, '\n');
			if (n) {
				*n = '\0';
			}
		}

		/* get modus */
		buf[LIST_SPACEPOS] = '\0';
		modus = (mode_t)atoi(buf);
		if (modus == 0) {
			fprintf(stderr, "** Corrupt entry in filelist\n");
			continue;
		}

		e = g_malloc(sizeof(struct entry));
		e->f_name = g_strdup(buf + LIST_SPACEPOS + 1);
		e->f_mode = modus;
		e->f_uid  = 0;
		e->f_gid  = 0;
		e->f_mtime = 0;
		g_tree_replace(tree, (gpointer) e, VALUE);
	}
	g_free(buf);
	return tree;
}

/**
 * return the m_time of the filelist
 */
time_t
mtime(char *f)
{
	struct stat s;

	if (lstat(f, &s) != 0) {
		return 0;
	}
	return s.st_mtime;
}

int 
main(int argc, char **argv) 
{
	GTree 	*backup; 	/* on disk stuff */
	GTree 	*remove;	/* what needs to be rm'd */
	GTree 	*curtree; 	/* previous backup tree */
	FILE 	*fplist;
	gint    i;
	int 	c;
	char 	*crawl;
	char    pwd[BUFSIZE + 1];

	curtree = g_tree_new(gfunc_equal);
	backup  = g_tree_new(gfunc_equal);
	remove  = NULL;
	opterr = 0;

	if (((getuid() != geteuid()) || (getgid() != getegid()))) {
		fprintf(stderr, "** For safety reasons " PROGNAME " will not run suid/sgid\n");
		exit(EXIT_FAILURE);
        }

	if (!getcwd(pwd, BUFSIZE)) {
		fprintf(stderr, "** Could not get current working directory\n");
		exit(EXIT_FAILURE);
	}

	while ((c = getopt (argc, argv, "hVnvx0")) != -1) {
		switch (c)
		{
			case 'h':
				usage(stdout);
				exit(EXIT_SUCCESS);
			case 'V':
				version(stdout);
				exit(EXIT_SUCCESS);
			case 'n':
				opt_nobackup = 0;
				break;
			case 'v':
				opt_verbose = 1;
				break;
			case 'x':
				opt_onefilesystem = 1;
				break;
			case '0':
				opt_null = 1;
				break;
		}
	}
	argc -= optind;
	argv += optind;

	if (argc < 2) {
		usage(stdout);
		exit(EXIT_FAILURE); 
	}

	/* Check for full of incremental dump */
	if ((list_mtime = mtime(argv[0])) == 0) {
		dumptype = NULL_DUMP;
	} else {
		dumptype = INC_DUMP;
	}

	if (!(fplist = fopen(argv[0], "a+"))) {
		fprintf(stderr, "** Could not open file\n");
		exit(EXIT_FAILURE);
	} else {
		rewind(fplist);
	}

	curtree = g_tree_read_file(fplist);

	for (i = 1; i < argc; i++) {
		if (argv[i][0] != '/') {
			crawl = g_strdup_printf("%s%c%s", pwd, DIR_SEP, argv[i]);
		} else {
			crawl = g_strdup(argv[i]);
		}

		/* add dirs leading up the backup dir */
		if (! dir_prepend(backup, crawl)) {
			exit(EXIT_FAILURE);
		}
		/* descend into the dark, misty directory */
		(void)dir_crawl(backup, crawl);
		g_free(crawl);
	}
	remove = g_tree_substract(curtree, backup); 
	
	/* first what to remove, then what to backup */
	g_tree_foreach(remove, gfunc_remove, NULL); 
	g_tree_foreach(backup, gfunc_backup, NULL);

	/* write new filelist */
	ftruncate(fileno(fplist), 0);  
	g_tree_foreach(backup, gfunc_write, fplist);
	fclose(fplist); 

	g_tree_foreach(curtree, gfunc_free, NULL);
	g_tree_foreach(backup, gfunc_free, NULL);
/*	g_tree_foreach(remove, gfunc_free, NULL); */
	
	/* I free too much... */
	g_tree_destroy(curtree);
	g_tree_destroy(backup);
#if 0
	g_tree_free(remove);
#endif
	
	exit(EXIT_SUCCESS);
}
