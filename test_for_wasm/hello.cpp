#include <emscripten/bind.h>
int add(int a, int b) {
    return a + b;
}
EMSCRIPTEN_BINDINGS(hello) {
    emscripten::function("add", &add);
}