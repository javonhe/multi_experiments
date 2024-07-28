#include "mini_lib.h"


int main(int argc, char *argv[])
{
    
    int fd = open(argv[1], O_CREAT | O_APPEND | O_RDWR, 0644);

    if (fd > 0)
    {
        write(fd, "hello world", 12);
        close(fd);
    }

    return 0;
}