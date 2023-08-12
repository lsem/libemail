#include <cstdio>

int main() {
    while (!feof(stdin)) {
        auto c = fgetc(stdin);
        if (c == '\r') {
            fputs("\\r", stdout);
        } else if (c == '\n') {
            fputs("\\n", stdout);
        } else {
            putc(c, stdout);
        }
    }
}
