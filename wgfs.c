/* This is a quick hacky virtual fuse-based filesystem to
 * work around a limitation in the WorldGuard Bukkit plugin
 * (for a Minecraft server).
 *
 * Tested on linux, build with:
 * gcc -Wall wgfs.c `pkg-config fuse --cflags --libs` -o wgfs
 */

#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>

char *metadata_path;

static int wgfuse_perms(const char *path, struct stat *stbuf) {

	if (strchr(path, '.') == NULL) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
	} else {
		stbuf->st_mode = S_IFREG | 0444;
		stbuf->st_nlink = 1;
		stbuf->st_size = 1024; /* dummy size */
	}

	return 0;
}

static int wgfuse_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {

	char realpath[1024];

	sprintf(realpath, "%s/%s", metadata_path, path);

	DIR * dir = opendir(realpath);
	struct dirent * dp;
	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);
	filler(buf, "blacklist.txt", NULL, 0);
	filler(buf, "config.yml", NULL, 0);
	filler(buf, "regions.yml", NULL, 0);
	while ((dp = readdir(dir)) != NULL) {
		if (dp->d_type == DT_DIR) {
			filler(buf, dp->d_name, NULL, 0);
		}
	}

	closedir(dir);
	return 0;
}

static int wgfuse_open(const char *path, struct fuse_file_info *fi) {
	return 0;
}

static int wgfuse_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {

	char realpath[255];
	int fd;
	int res;

	sprintf(realpath, "%s/%s", metadata_path, path);

	if(access(realpath, F_OK) == -1) {
		// File not found, use global file
		sprintf(realpath, "%s/%s", metadata_path, strrchr(realpath, '/'));
	}

	if ((fd = open(realpath, O_RDONLY)) == -1) {
		return -ENOENT;
	}

	if ((res = pread(fd, buf, size, offset)) == -1) {
		close(fd);
		return -ENOENT;
	}

	close(fd);
	return res;
}

static struct fuse_operations wgfuse_operations = {
	.getattr	= wgfuse_perms,
	.readdir	= wgfuse_readdir,
	.open		= wgfuse_open,
	.read		= wgfuse_read
};

int main(int argc, char *argv[]) {
	if (argc < 3) {
		fprintf(stderr, "usage: <mount point> <template-path>\n");
		return -1;
	}

	metadata_path = argv[argc - 1];

	return fuse_main(argc - 1, argv, &wgfuse_operations, NULL);
}
