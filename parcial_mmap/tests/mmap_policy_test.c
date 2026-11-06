#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/types.h>

static void die(const char *what) { perror(what); exit(EXIT_FAILURE); }

int main(int argc, char **argv)
{
    char path[] = "/tmp/mmap-guard-test-XXXXXX";
    int fd;
    struct stat before, after;
    void *p;

    (void)argc;
    (void)argv;
    fd = mkstemp(path);
    if (fd < 0) die("mkstemp");
    if (fchown(fd, 0, 0) < 0 && geteuid() == 0) die("fchown");
    if (fstat(fd, &before) < 0) die("fstat");
    if (before.st_size < 4096 && ftruncate(fd, 4096) < 0) die("ftruncate");

    p = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
    if (p == MAP_FAILED) die("MAP_PRIVATE");
    ((char *)p)[0] = 'P';
    if (munmap(p, 4096) < 0) die("munmap private");

    p = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (p == MAP_FAILED) {
        printf("MAP_SHARED writable: denied (%s)\n", strerror(errno));
    } else {
        ((char *)p)[0] = 'S';
        if (msync(p, 4096, MS_SYNC) < 0) die("msync");
        munmap(p, 4096);
        printf("MAP_SHARED writable: allowed (policy inactive or file not protected)\n");
    }

    if (fstat(fd, &after) < 0) die("fstat after");
    printf("file size before=%ld after=%ld; private write is COW\n",
           (long)before.st_size, (long)after.st_size);
    close(fd);
    unlink(path);
    return 0;
}
