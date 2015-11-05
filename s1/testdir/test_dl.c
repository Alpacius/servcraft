#include    <unistd.h>

void printmsg(void) {
    write(STDOUT_FILENO, "libver2\n", sizeof("libver1"));
}
