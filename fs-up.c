/* 
 * Copyright (c) 2009 Miek Gieben
 * See LICENSE for the license
 * rdup-up -- update an directory tree with 
 * and rdup archive
 */

#include "rdup-up.h"

extern sig_atomic_t sig;
extern GSList *hlink;

/* signal.c */
void got_sig(int signal);

gboolean
mk_link(struct r_entry *e, gboolean exists, char *s, char *t, char *p)
{
	/* there is something */
	if (exists) {
		fprintf(stderr, "removing %s\n", s);	
		(void)rm(s);
	}

	/* symlink */
	if (S_ISLNK(e->f_mode)) {
		fprintf(stderr, "s %s||%s\n", s, t);
		if (symlink(t, s) == -1) {
			msg("Failed to make symlink: `%s -> %s\': %s", s, t, strerror(errno));
			return FALSE;
		}
		return TRUE;
	}

	/* hardlink */
	/* make target also fall in the backup dir */
	t = g_strdup_printf("%s%s", p, s + e->f_size + 4);
	e->f_name = g_strdup_printf("%s -> %s", s, t);
	e->f_size = strlen(s);
	e->f_name_size = strlen(e->f_name);
	hlink = g_slist_append(hlink, e);
	return TRUE;
}

gboolean
mk_reg(FILE *in, struct r_entry *e, gboolean exists)
{
	FILE *out;
	char *buf;
	size_t   i, mod, rest;

	/* there is something */
	if (exists) {
		fprintf(stderr, "remving %s\n", e->f_name);	
		(void)rm(e->f_name);
	}

	if (!(out = fopen(e->f_name, "w"))) {
		msg("Failed to open file `%s\': %s", e->f_name, strerror(errno));
		if (!(out = fopen("/dev/null", "w"))) {
			msg("Failed to open `/dev/null\': %s", strerror(errno));
			exit(EXIT_FAILURE);
		}
	} else {
		/* set permissions right away */
		g_chmod(e->f_name, e->f_mode);
	}

	buf   = g_malloc(BUFSIZE + 1);
	rest = e->f_size % BUFSIZE;	      /* then we need to read this many */
	mod  = (e->f_size - rest) / BUFSIZE;  /* main loop happens mod times */

	/* mod loop */
	for(i = 0; i < mod; i += BUFSIZE) {
		i = fread(buf, sizeof(char), BUFSIZE, in);
		if (fwrite(buf, sizeof(char), i, out) != i) {
			msg(_("Write failure `%s\': %s"), e->f_name, strerror(errno));
			fclose(out);
			return FALSE;
		}
	}
	/* rest */
	i = fread(buf, sizeof(char), rest, in);
	if (fwrite(buf, sizeof(char), i, out) != i) {
		msg(_("Write failure `%s\': %s"), e->f_name, strerror(errno));
		fclose(out);
		return FALSE;
	}
	return TRUE;
}

gboolean
mk_dir(struct r_entry *e, struct stat *st, gboolean exists) 
{
	/* there is something and it's a dir */
	if (exists && S_ISDIR(st->st_mode)) {
		g_chmod(e->f_name, e->f_mode);
		return TRUE;
	}

	fprintf(stderr, "remove %s", e->f_name);

	if (g_mkdir(e->f_name, e->f_mode) == -1)
		return FALSE;

	g_chmod(e->f_name, e->f_mode);
	return TRUE;
}


/* make an object in the filesystem */
gboolean
mk_obj(FILE *in, char *p, struct r_entry *e) 
{
	char     *s, *t;
	gboolean exists;
	struct stat st;

	/* devices sockets and other fluf! */

	if (lstat(e->f_name, &st) == -1) 
		exists = FALSE;
	else
		exists = TRUE;

	switch(e->plusmin) {
		case '-':
			/* remove all stuff you can find */
			if (S_ISLNK(e->f_mode) || e->f_lnk) {
				/* get out the source name */
				s = e->f_name;
				s[e->f_size] = '\0';
			} else {
				s = e->f_name;
			}
			(void) rm(s);
			return TRUE;
		case '+':
			if (S_ISDIR(e->f_mode)) {
				(void) mk_dir(e, &st, exists);	
				break;
			}

			/* no, first sym and hardlinks and then 
			 * a regular file 
			 */

			if (S_ISLNK(e->f_mode) || e->f_lnk) {
				/* get out the source name and re-stat it */
				s = e->f_name;
				s[e->f_size] = '\0';
				t = s + e->f_size + 4; /* ' -> ' */

				if (lstat(s, &st) == -1) 
					exists = FALSE;
				else
					exists = TRUE;

				(void)mk_link(e, exists, s, t, p);
				break;
			}


			if (S_ISREG(e->f_mode)) {
				mk_reg(in, e, exists);
				break;
			}
#if 0
			if (S_ISSOCK(e->fmode)) {
				mk_sock(e, exists);
				break;
			}
#endif
	}
	return TRUE;
}

/* Create the remaining hardlinks in the target directory */
gboolean
mk_hlink(GSList *h)
{
	struct r_entry *e;
	GSList *p;
	char *s, *t;
	for (p = g_slist_nth(h, 0); p; p = p->next) { 
		e = (struct r_entry *)p->data;

		s = e->f_name;
		s[e->f_size] = '\0';
		t = s + e->f_size + 4; /* ' -> ' */
		if (link(t, s) == -1) {
			msg("Failed to create hardlink `%s -> %s\': %s",
					s, t, strerror(errno));
			return FALSE;
		}
	}
	return TRUE;
}
