/*
 * ic0.c -- inode churner
 *
 * Motivation: https://bugzilla.redhat.com/show_bug.cgi?id=1066751.
 *
 * 2017 Jon Simons
 */

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define USAGE \
	"Usage: ic0 <path>\n" \
	"\n" \
	"Where inode number zero will be sought by brute force creation of\n" \
	"empty files directly under <path>.\n"

static
void usage(void)
{
	fprintf(stderr, USAGE);
	exit(EXIT_FAILURE);
}

static
void parse_args(int argc, char **argv, char *dest_file)
{
	int r = 0;
	const char *dest_dir = NULL;
	pid_t pid = getpid();

	if (argc != 2) {
		usage();
	}

	dest_dir = argv[1];
	if (access(dest_dir, W_OK) != 0) {
		perror("can not write to dest_dir");
		usage();
	}

	r = snprintf(dest_file, PATH_MAX - 1, "%s/ic-%d-churnfile", dest_dir, pid);
	if ((r < 0) || (r >= PATH_MAX - 1)) {
		fprintf(stderr, "error: unexpected failure to generate "
		                "churn file name, exiting\n");
		exit(EXIT_FAILURE);
	}

	/* unlink possible leftovers */
	(void) unlink(dest_file);

	return;
}

enum churn_result { CHURN_FOUND = 0, CHURN_MISS = 1, CHURN_WRAP = 2 };

static
enum churn_result churn_inode(const char *churn_file_name,
                              unsigned long long *inode_out)
{
	int r = 0;
	int fd = -1;
	struct stat s;

	fd = open(churn_file_name, O_WRONLY | O_CREAT | O_EXCL);
	if (r == -1) {
		perror("failed to create churn file");
		fprintf(stderr, "error: unexpected failure to create "
		                "churn file '%s', exiting\n",
		                churn_file_name);
		exit(EXIT_FAILURE);
	}

	r = close(fd);
	if (r != 0) {
		perror("failed to close churn file");
		fprintf(stderr, "error: unexpected failure to close "
		                "churn file '%s', exiting\n",
		                churn_file_name);
		goto errout;
	}

	r = stat(churn_file_name, &s);
	if (r != 0) {
		perror("failed to stat churn file");
		fprintf(stderr, "error: unexpected failure to stat "
		                "churn file '%s', exiting\n",
		                churn_file_name);
		goto errout;
	}

	*inode_out = (unsigned long long) s.st_ino;
	if (s.st_ino == 0) {
		return CHURN_FOUND;
	}

errout:
	r = unlink(churn_file_name);
	if (r != 0) {
		perror("failed to unlink churn file");
		fprintf(stderr, "error: unexpected failure to stat "
		                "churn file '%s', exiting\n",
		                churn_file_name);
		exit(EXIT_FAILURE);
	}

	return CHURN_MISS;
}

int main(int argc, char **argv)
{
	int r = 0;
	char dest_file[PATH_MAX] = { 0 };
	enum churn_result cr = CHURN_MISS;
	bool first_inode_initialized = false;
	unsigned long long first_inode = 0UL;
	unsigned long long inode_out = 0UL;
	unsigned long long iterations = 0UL;

	parse_args(argc, argv, dest_file);

	do {
		cr = churn_inode(dest_file, &inode_out);
		if (!first_inode_initialized) {
			first_inode_initialized = true;
			first_inode = inode_out;
		} else if (inode_out <= first_inode) {
			cr = CHURN_WRAP;
		}

		iterations += 1;
		if ((iterations % 1000000) == 0) {
			fprintf(stderr, "current inode number: %lld\n", inode_out);
			fflush(stderr);
		}
	} while (cr == CHURN_MISS);

	if (cr == CHURN_WRAP) {
		fprintf(stderr, "FAILED: last generated inode number %lld "
		                "wrapped first one (%lld), exiting\n",
		                inode_out, first_inode);
		exit(EXIT_FAILURE);
	}

	printf("OK: path '%s' has inode number %lld\n", dest_file, inode_out);
	return 0;
}
