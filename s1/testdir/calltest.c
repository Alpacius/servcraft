#include    <stdio.h>

void f(void) {
    printf("hello\n");
}

int main(void) {
    void *p = f;
    ((void (*)(void)) p)();
    return 0;
}
