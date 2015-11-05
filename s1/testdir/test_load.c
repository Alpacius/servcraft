#include    <stdio.h>
#include    <dlfcn.h>

int main(void) {
    void *dlhandle1 = dlopen("./libtestdl.so.1", RTLD_NOW);
    void (*sym1)(void) = (void (*)(void)) dlsym(dlhandle1, "printmsg");
    sym1();
    getchar();
    void *dlhandle2 = dlopen("./libtestdl.so.2", RTLD_NOW);
    void (*sym2)(void) = (void (*)(void)) dlsym(dlhandle2, "printmsg");
    sym1();
    dlclose(dlhandle1);
    sym2();
    dlclose(dlhandle2);
    return 0;
}
