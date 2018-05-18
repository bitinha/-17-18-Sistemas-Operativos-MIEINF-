
#include <unistd.h>

struct buffer_t{
    int fd;
    char* buf;
    size_t nbyte, porler;
    int cursor;
};

int create_buffer(int fildes, struct buffer_t *buffer, size_t nbyte);

int destroy_buffer(struct buffer_t *buffer);

ssize_t readln(struct buffer_t *buffer, char **buf);