#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>
#include "vi.h"

#define CR2L		"ءآأؤإئابةتثجحخدذرزسشصضطظعغـفقكلمنهوىييپچژکگی‌‍؛،»«؟"
#define CNEUT		"-!\"#$%&'()*+,./:;<=>?@^_`{|}~ "

static struct dcontext {
	int dir;
	char *pat;
} dcontexts[] = {
	{-1, "^[" CR2L "]"},
	{+1, "^[a-zA-Z_0-9]"},
};

/* direction marks */
static struct dmark {
	int ctx;	/* the direction context for this mark; 0 means any */
	int dir;	/* the direction of matched text */
	int grp;	/* the nested subgroup; 0 means no groups */
	char *pat;	/* the pattern */
} dmarks[] = {
	{+0, +1, 0, "$([^$]+)\\$"},
	{+0, +1, 1, "\\\\\\*\\[([^]]+)\\]"},
	{+1, -1, 0, "[" CR2L "][" CNEUT CR2L "]*[" CR2L "]"},
	{-1, +1, 0, "[a-zA-Z0-9_][^" CR2L "\\\\`$']*[a-zA-Z0-9_]"},
};

static struct reset *dir_rslr;
static struct reset *dir_rsrl;
static struct reset *dir_rsctx;

static int uc_off(char *s, int off)
{
	char *e = s + off;
	int i;
	for (i = 0; s < e && *s; i++)
		s = uc_next(s);
	return i;
}

static int dir_match(char **chrs, int beg, int end, int ctx, int *rec,
		int *r_beg, int *r_end, int *c_beg, int *c_end, int *dir)
{
	int subs[16 * 2];
	struct reset *rs = ctx < 0 ? dir_rsrl : dir_rslr;
	struct sbuf *str = sbuf_make();
	int found;
	sbuf_mem(str, chrs[beg], chrs[end] - chrs[beg]);
	found = reset_find(rs, sbuf_buf(str), LEN(subs) / 2, subs, 0);
	if (found >= 0 && r_beg && r_end && c_beg && c_end) {
		struct dmark *dm = &dmarks[found];
		char *s = sbuf_buf(str);
		int grp = dm->grp;
		*r_beg = beg + uc_off(s, subs[0]);
		*r_end = beg + uc_off(s, subs[1]);
		*c_beg = subs[grp * 2 + 0] >= 0 ? beg + uc_off(s, subs[grp * 2 + 0]) : *r_beg;
		*c_end = subs[grp * 2 + 1] >= 0 ? beg + uc_off(s, subs[grp * 2 + 1]) : *r_end;
		*dir = dm->dir;
		*rec = grp > 0;
	}
	sbuf_free(str);
	return found < 0;
}

static void dir_reverse(int *ord, int beg, int end)
{
	end--;
	while (beg < end) {
		int tmp = ord[beg];
		ord[beg] = ord[end];
		ord[end] = tmp;
		beg++;
		end--;
	}
}

/* reorder the characters based on direction marks and characters */
static void dir_fix(char **chrs, int *ord, int dir, int beg, int end)
{
	int r_beg, r_end, c_beg, c_end;
	int c_dir, c_rec;
	while (beg < end && !dir_match(chrs, beg, end, dir, &c_rec,
				&r_beg, &r_end, &c_beg, &c_end, &c_dir)) {
		if (dir < 0)
			dir_reverse(ord, r_beg, r_end);
		if (c_dir < 0)
			dir_reverse(ord, c_beg, c_end);
		if (c_beg == r_beg)
			c_beg++;
		if (c_rec)
			dir_fix(chrs, ord, c_dir, c_beg, c_end);
		beg = r_end;
	}
}

int dir_context(char *s)
{
	int found;
	if (xdir == 'L')
		return +1;
	if (xdir == 'R')
		return -1;
	found = reset_find(dir_rsctx, s ? s : "", 0, NULL, 0);
	if (found >= 0)
		return dcontexts[found].dir;
	return xdir == 'r' ? +1 : -1;
}

/* reorder the characters in s */
void dir_reorder(char *s, int *ord)
{
	int n;
	char **chrs = uc_chop(s, &n);
	int dir = dir_context(s);
	if (n && chrs[n - 1][0] == '\n') {
		ord[n - 1] = n - 1;
		n--;
	}
	dir_fix(chrs, ord, dir, 0, n);
	free(chrs);
}

void dir_init(void)
{
	char *relr[128];
	char *rerl[128];
	char *ctx[128];
	int i;
	for (i = 0; i < LEN(dmarks); i++) {
		relr[i] = dmarks[i].ctx >= 0 ? dmarks[i].pat : NULL;
		rerl[i] = dmarks[i].ctx <= 0 ? dmarks[i].pat : NULL;
	}
	dir_rslr = reset_make(LEN(dmarks), relr);
	dir_rsrl = reset_make(LEN(dmarks), rerl);
	for (i = 0; i < LEN(dcontexts); i++)
		ctx[i] = dcontexts[i].pat;
	dir_rsctx = reset_make(LEN(dcontexts), ctx);
}

void dir_done(void)
{
	reset_free(dir_rslr);
	reset_free(dir_rsrl);
	reset_free(dir_rsctx);
}