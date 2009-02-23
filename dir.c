/* 
 * Copyright (c) 2009 Miek Gieben
 * See LICENSE for the license
 * make a dir writable and later remove that
 * right again
 */

#include "rdup-up.h"

struct stat *
dir_write(gchar *p)
{
	 /* chmod +w . && rm $file && chmod -w #and hope for the best */
	struct stat *s = g_malloc(sizeof(struct stat));

	if (!p)
		return NULL;

	if (stat(p, s) == -1)
		return NULL;

	/* make it writable, assume we are the OWNER */
	chmod(p, s->st_mode | S_IWUSR);
	return s;
}

void
dir_restore(gchar *p, struct stat *s)
{
	if (!s || !p)
		return;
	/* restore perms - assumes *s has not be f*cked up */
	chmod(p, s->st_mode);
}

/**
 * return parent dir string as in p/..
 */
gchar *
dir_parent(gchar *p)
{
	gchar *p2;
	p2 = g_strdup_printf("%s/%s", p, "..");
	return p2;
}
