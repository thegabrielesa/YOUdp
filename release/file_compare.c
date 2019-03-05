#include <stdio.h> 
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/types.h> 
#include <sys/socket.h> 
#include <arpa/inet.h>
#include <time.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#include "types.h"
#include "basic.h"

int main(int argc, char *argv[]) {
    int res;
    char byte1, byte2;
    int fd1, fd2;
    off_t fs1, fs2, i;
    
    abort_on_error2(argc != 3, "usage: ./file_compare <path1> <path2>");
    
    /* open files */
    fd1 = open_file(argv[1], "", O_RDONLY);
    abort_on_error2(fd1 == -1, "error: first file does not exist");

    fd2 = open_file(argv[2], "", O_RDONLY);
    abort_on_error2(fd2 == -1, "error: second file does not exist");

    /* compare file sizes */
    fs1 = get_file_size(fd1);
    fs2 = get_file_size(fd2);
    abort_on_error2(fs1 != fs2, "error: the two files have different length");

    /* compare byte by byte */
    i = (off_t) 0;
    while (i < fs1) {
        res = full_read(fd1, &byte1, 1);
        abort_on_error(res != 0, "error in read");

        res = full_read(fd2, &byte2, 1);
        abort_on_error(res != 0, "error in 2nd read");

        abort_on_error2(byte1 != byte2, "*** THE FILES ARE DIFFERENT ***");
        i++;
    }

    printf("*** THE FILES ARE EQUAL ***\n");

    return EXIT_SUCCESS;
}
