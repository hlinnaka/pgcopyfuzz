/*
 * Generates files for COPY FROM testing, 
 *
 */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static char *dict[] = {
	"a",
	"111111111122222222223333333333",
	"\xE8\xB1\xA1",
	"\xE8\xB1",
	"\n",
	"\r",
	"\\",
	".",
	NULL
};

/* Number of tokens in each generated file */
#define LEN 5

int
main(int argc, char **argv)
{
	int			x[LEN] = { 0 };
	char	   *outputdir;

	if (argc < 2)
	{
		fprintf(stderr, "usage: gencopyfuzz <outputdir>\n");
		exit(1);
	}
	outputdir = argv[1];

	if (chdir(outputdir) < 0)
	{
		fprintf(stderr, "could not change directory to \"%s\": %s",
			outputdir, strerror(errno));
		exit(1);
	}

	for (;;)
	{
		int			i;
		char		fname[100 + LEN];
		char	   *f;
		FILE	   *fp;

		/* construct file name */
		snprintf(fname, sizeof(fname), "gencopyfuzz-");
		f = fname + strlen(fname);
		for (i = 0; i < LEN; i++)
		{
			f[i] = '0' + x[i];
		}
		f[LEN] = '\0';

		/* generate the output to file */
		fp = fopen(fname, "wb");
		for (i = 0; i < LEN; i++)
		{
			char *w = dict[x[i]];
			fwrite(w, strlen(w), 1, fp);
		}
		fclose(fp);

		/* increment filename */
		for (i = 0; i < LEN; i++)
		{
			x[i]++;
			if (dict[x[i]] == NULL)
			{
				x[i] = 0;
				continue;
			}
			break;
		}

		if (i == LEN)
		{
			printf("all done!\n");
			exit(1);
		}
	}
}
