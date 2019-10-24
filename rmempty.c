// Copyright 2019 Lassi Kortela
// SPDX-License-Identifier: ISC

#include <sys/types.h>

#include <sys/stat.h>

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#define PROGNAME "rmempty"

#ifndef PROGVERSION
#define PROGVERSION "unknown"
#endif

static int
walkent(void);

static char path[4096];
static struct stat st;
static int vflags;
static int lflag;
static int fflag;
static int helpflag;
static int versionflag;
static int delete_things;

static void
usage(FILE *stream, int exitcode)
{
	fprintf(stream, "usage: %s [-flv] file ...\n", PROGNAME);
	exit(exitcode);
}

static void
version(void)
{
	printf("%s version %s\n", PROGNAME, PROGVERSION);
	exit(0);
}

static void
die(const char *msg)
{
	fprintf(stderr, "%s\n", msg);
	exit(2);
}

static void
die_s(const char *msg, const char *arg)
{
	fprintf(stderr, "%s: %s\n", msg, arg);
	exit(2);
}

static void
diesys_s(const char *msg, const char *arg)
{
	fprintf(stderr, "%s: %s: %s\n", msg, arg, strerror(errno));
	exit(2);
}

static char *
path_basename(void)
{
	char *pos;

	pos = strchr(path, 0);
	while (pos > path) {
		if (pos[-1] == '/')
			break;
	}
	return pos;
}

static size_t
path_append(const char *s)
{
	char *pos;
	size_t oldlen, len;

	pos = strchr(path, 0);
	oldlen = pos - path;
	len = strlen(s);
	if (1 + len >= sizeof(path) - oldlen) {
		die("path too long");
	}
	*pos++ = '/';
	memcpy(pos, s, len);
	return oldlen;
}

static void
path_set(const char *s)
{
	size_t len;

	len = strlen(s);
	while (len > 1) {
		if (s[len - 1] != '/')
			break;
		len--;
	}
	if (len >= sizeof(path)) {
		die("path too long");
	}
	memset(path, 0, sizeof(path));
	memcpy(path, s, len);
}

static void
path_truncate(size_t len)
{
	memset(path + len, 0, sizeof(path) - len);
}

static void
rmemptyfile(void)
{
	if (vflags >= 1) {
		printf("deleting %s\n", path);
	}
	if (unlink(path) == -1) {
		diesys_s("cannot delete empty file", path);
	}
}

static void
rmemptydir(void)
{
	if (vflags >= 1) {
		printf("deleting %s\n", path);
	}
	if (rmdir(path) == -1) {
		diesys_s("cannot delete empty directory", path);
	}
}

typedef int (*match_func_t)(const unsigned char *b, size_t nbyte);

static int
is_mac_file_settings(const unsigned char *b, size_t nbyte)
{
	if (nbyte < 4) {
		return 0;
	}
	return (b[0] == 0x00) && (b[1] == 0x05) && (b[2] == 0x16) &&
	       (b[3] == 0x07);
}

static int
is_mac_folder_settings(const unsigned char *b, size_t nbyte)
{
	if (nbyte < 4) {
		return 0;
	}
	return !memcmp(b, "Bud1", 4);
}

static int
is_windows_thumbs_db(const unsigned char *b, size_t nbyte)
{
	const unsigned char at512[4] = {0xfd, 0xff, 0xff, 0xff};
	const unsigned char at524[4] = {0x04, 0x00, 0x00, 0x00};

	if (nbyte < 524 + 4) {
		return 0;
	}
	return !memcmp(&b[512], at512, 4) && !memcmp(&b[524], at524, 4);
}

static int
iscachefile()
{
	static unsigned char data[1024];
	ssize_t nread;
	int fd;
	match_func_t match;
	const char *name;

	name = path_basename();
	if ((name[0] == '.') && (name[1] == '_')) {
		match = is_mac_file_settings;
	} else if (!strcasecmp(name, ".DS_Store")) {
		match = is_mac_folder_settings;
	} else if (!strcasecmp(name, "Thumbs.db")) {
		match = is_windows_thumbs_db;
	} else {
		return 0;
	}
	if (!(fd = open(path, O_RDONLY))) {
		diesys_s("cannot open file", path);
	}
	if ((nread = read(fd, data, sizeof(data))) == (ssize_t)-1) {
		diesys_s("cannot read from file", path);
	}
	close(fd);
	return match(data, (size_t)nread);
}

static int
isblankfile(size_t filesize)
{
	ssize_t nread;
	int fd;
	char bytes[4];

	if (filesize < 1) {
		return 1;
	}
	if (filesize > 2) {
		return 0;
	}
	if (!(fd = open(path, O_RDONLY))) {
		diesys_s("cannot open file", path);
	}
	if ((nread = read(fd, bytes, filesize)) == (ssize_t)-1) {
		diesys_s("cannot read from file", path);
	}
	if ((size_t)nread != filesize) {
		die_s("short read from", path);
	}
	close(fd);
	if (filesize == 1) {
		return (bytes[0] == '\r') || (bytes[0] == '\n');
	}
	if (filesize == 2) {
		return (bytes[0] == '\r') && (bytes[1] == '\n');
	}
	return 0;
}

static int
isemptyfile(void)
{
	if (!S_ISREG(st.st_mode)) {
		return 0;
	}
	if (!st.st_size) {
		return 1;
	}
	if (isblankfile(st.st_size)) {
		return 1;
	}
	if (iscachefile()) {
		return 1;
	}
	return 0;
}

static int
walkdir(void)
{
	DIR *handle;
	struct dirent *d;
	size_t oldlen;
	int firsterror, isempty;

	if (!(handle = opendir(path))) {
		goto done;
	}
	for (;;) {
		errno = 0;
		if (!(d = readdir(handle)))
			break;
		if (!strcmp(d->d_name, ".") || !strcmp(d->d_name, ".."))
			continue;
		oldlen = path_append(d->d_name);
		isempty = walkent();
		path_truncate(oldlen);
		if (!isempty)
			return 0;
	}
done:
	firsterror = errno;
	if (handle && (closedir(handle) == -1) && !firsterror)
		firsterror = errno;
	if ((errno = firsterror))
		diesys_s("cannot list directory", path);
	printf("empty directory: %s\n", path);
	if (delete_things) {
		rmemptydir();
	}
	return 1;
}

static int
walkent(void)
{
	if (lstat(path, &st) == -1) {
		diesys_s("cannot stat", path);
	}
	if (S_ISDIR(st.st_mode)) {
		return walkdir();
	} else if (isemptyfile()) {
		printf("empty file: %s\n", path);
		if (delete_things) {
			rmemptyfile();
		}
		return 1;
	}
	return 0;
}

int
main(int argc, char **argv)
{
	const char *arg;
	int ch;

	while ((ch = getopt(argc, argv, "fhlvV")) != -1) {
		switch (ch) {
		case 'f':
			fflag = 1;
			break;
		case 'h':
			helpflag = 1;
			break;
		case 'l':
			lflag = 1;
			break;
		case 'v':
			vflags++;
			break;
		case 'V':
			versionflag = 1;
			break;
		default:
			usage(stderr, 2);
			break;
		}
	}
	if (helpflag) {
		usage(stdout, 0);
	} else if (versionflag) {
		version();
	}
	argc -= optind;
	argv += optind;
	for (; (arg = *argv); argv++) {
		path_set(arg);
		if (fflag) {
			delete_things = 1;
			walkent();
		} else if (lflag) {
			delete_things = 0;
			walkent();
		} else {
			delete_things = 0;
			if (walkent()) {
				delete_things = 1;
				walkent();
			}
		}
	}
	return 0;
}
