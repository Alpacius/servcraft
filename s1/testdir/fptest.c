void f(void) {
}

void (*g(int a))(void) {
    return f;
}

void h(void) {
    g(1)();
}
