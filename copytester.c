/*
 * Test COPY FROM using files in corpus.
 *
 * For each file in the input directory (corpus), this calls
 * "COPY copytest FROM STDIN" and sends the file to the server.
 * If the command throws an error, like many of the input files do,
 * it is printed out. After processing each file, the contents
 * of the table are SELECTed and printed out.
 *
 * This is useful for testing a patch for regressions:
 * Run copytester against a patched and unpatched server, and compare
 * that the results are the same (except for acceptable differences
 * in error messages).
 */
#include "c.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libpq-fe.h"

/*
 * How many bytes to send to the server in one message?
 *
 * We intentionally read and send the the file one byte at a time to the
 * server. That's highly inefficient, of course, but it gives a better
 * chance of uncovering bugs in look-ahead around buffer boundaries.
 * But can make READSIZE larger if you don't want that.
 */
#define READSIZE 1


static void sendCopyFile(char *path, PGconn *conn);

static int
cmp_string(const void *a, const void *b)
{
	const char *astr = *(const char **) a;
	const char *bstr = *(const char **) b;

	return strcmp(astr, bstr);
}

int
main(int argc, char **argv)
{
	const char *inputdir;
	const char *conninfo;
	PGconn	   *conn;
	DIR		   *dir;
	struct dirent *de;
	PGresult   *res;
	char	  **filenames;
	int			nallocated;
	int			nfiles;

	if (argc < 3)
	{
		fprintf(stderr, "usage: copytester <inputdir> <connstring>\n");
		exit(1);
	}
	
	inputdir = argv[1];
	conninfo = argv[2];

	dir = opendir(inputdir);
	if (!dir)
	{
		fprintf(stderr, "could not open directory \"%s\": %s",
			inputdir, strerror(errno));
		exit(1);
	}

	nallocated=100;
	filenames = malloc(nallocated * sizeof(char *));
	if (!filenames)
	{
		fprintf(stderr, "out of memory\n");
		exit(1);
	}
	nfiles = 0;
	while ((de = readdir(dir)) != NULL)
	{
		char		fn[1000];

		if (strcmp(de->d_name, ".") == 0 ||
			strcmp(de->d_name, "..") == 0)
			continue;

		if (nfiles == nallocated)
		{
			nallocated *= 2;
			filenames = realloc(filenames, nallocated * sizeof(char *));
			if (!filenames)
			{
				fprintf(stderr, "out of memory\n");
				exit(1);
			}
		}

		snprintf(fn, sizeof(fn), "%s/%s", inputdir, de->d_name);
		filenames[nfiles++] = strdup(fn);
	}
	closedir(dir);

	/*
	 * Sort the filenames, so that you get deterministic output that you
	 * can easily compare between two runs.
	 */
	qsort(filenames, nfiles, sizeof(char *), cmp_string);

	conn = PQconnectdb(conninfo);
	if (PQstatus(conn) != CONNECTION_OK)
	{
		fprintf(stderr, "Connection to database failed: %s",
				PQerrorMessage(conn));
		exit(1);
	}

	printf("connected!\n");

	res = PQexec(conn, "CREATE TEMP TABLE copytest (t text)");
	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		printf("CREATE TABLE failed: %s\n", PQerrorMessage(conn));
		exit(1);
	}
	PQclear(res);

	for (int i = 0; i < nfiles; i++)
	{
		printf("file: %s\n", filenames[i]);

		sendCopyFile(filenames[i], conn);
	}

	
	res = PQexec(conn, "select * from copytest");
	if (PQresultStatus(res) != PGRES_TUPLES_OK)
	{
		printf("SELECT failed: %s\n", PQerrorMessage(conn));
		exit(1);
	}

	printf("\ntable contents:\n");
	
	/* next, print out the rows */
	for (int i = 0; i < PQntuples(res); i++)
	{
		printf("%s\n", PQgetvalue(res, i, 0));
	}

	PQclear(res);
}

static void
sendCopyFile(char *path, PGconn *conn)
{
	FILE	   *fp;
	char		buf[100];
	int			n;
	bool		OK = true;
	PGresult   *res;

	res = PQexec(conn, "COPY copytest FROM STDIN");
	if (PQresultStatus(res) != PGRES_COPY_IN)
	{
		fprintf(stderr, "COPY command failed: %s\n", PQerrorMessage(conn));
		exit(1);
	}
	PQclear(res);


	/*
	 * Send the file to the server.
	 */
	fp = fopen(path, "rb");
	if (!fp)
	{
		fprintf(stderr, "could not open file \"%s\"\n", path);
		exit(1);
	}

	for (;;)
	{
		n = fread(buf, READSIZE, 1, fp);

		if (n == 0)
		{
			if (feof(fp))
				break;
			fprintf(stderr, "error reading file \"%s\"\n", path);
			exit(1);
		}
		
		if (PQputCopyData(conn, buf, n) <= 0)
		{
			OK = false;
			break;
		}
	}

	fclose(fp);

	if (PQputCopyEnd(conn, OK ? NULL : "aborted because of read failure") <= 0)
	{
		OK = false;
	}

	/*
	 * Check command status and return to normal libpq state.
	 */
	while (res = PQgetResult(conn), PQresultStatus(res) == PGRES_COPY_IN)
	{
		OK = false;
		PQclear(res);
		/* We can't send an error message if we're using protocol version 2 */
		PQputCopyEnd(conn, "trying to exit copy mode");
	}
	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		char		*f;

		f = PQresultErrorField(res, PG_DIAG_MESSAGE_PRIMARY);
		printf("COPY failed: %s\n", f);

		f = PQresultErrorField(res, PG_DIAG_MESSAGE_DETAIL);
		if (f)
			printf("DETAIL: %s\n", f);

		f = PQresultErrorField(res, PG_DIAG_MESSAGE_HINT);
		if (f)
			printf("HINT: %s\n", f);

		f = PQresultErrorField(res, PG_DIAG_CONTEXT);
		if (f)
			printf("CONTEXT: %s\n", f);

		OK = false;
	}
	PQclear(res);
}
