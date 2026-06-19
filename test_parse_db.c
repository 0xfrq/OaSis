#include <stdio.h>
#include <string.h>

static int streq(const char *a, const char *b) { return strcmp(a,b) == 0; }
static int parse_int(const char *s, unsigned int *out) {
    int v = 0;
    while (*s >= '0' && *s <= '9') { v = v * 10 + (*s - '0'); s++; }
    *out = v; return 1;
}

int main() {
    char ops[] = "109, 111, 114, 110, 105, 110, 103, 32, 115, 117, 110, 115, 104, 105, 110, 101, 101, 101, 101, 101, 33, 0";
    char *p = ops;
    while (*p) {
        while (*p == ' ' || *p == '\t' || *p == ',') p++;
        if (!*p) break;
        char *end = p;
        while (*end && *end != ',') end++;
        char *trimmed = end;
        while (trimmed > p && (trimmed[-1] == ' ' || trimmed[-1] == '\t')) trimmed--;
        char save = *trimmed;
        *trimmed = 0;
        unsigned int val;
        if (parse_int(p, &val)) {
            printf("val: %d\n", val);
        }
        *trimmed = save;
        p = end;
    }
    return 0;
}
