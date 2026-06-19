#include <libc/stdio.h>

int main() {
    // Test printf with various format specifiers
    printf("=== printf Tests ===\n");

    // Basic types
    printf("Character: %c\n", 'A');
    printf("String: %s\n", "Hello World");
    printf("Integer: %d\n", -123);
    printf("Unsigned: %u\n", 456);
    printf("Hex (lower): %x\n", 0x123abc);
    printf("Hex (upper): %X\n", 0x123abc);
    printf("Octal: %o\n", 0123);
    printf("Percent: %%\n");

    // Pointer
    int x = 42;
    printf("Pointer: %p\n", &x);

    // Formatting
    printf("Width: '%10s'\n", "test");
    printf("Zero-pad: %05d\n", 42);
    printf("Left-align: '%-10s'\n", "test");

    // Test scanf with various format specifiers
    printf("\n=== scanf Tests ===\n");
    printf("Enter an integer: ");

    int num;
    if (scanf("%d", &num) == 1) {
        printf("You entered: %d\n", num);
    }

    printf("Enter a string: ");
    char str[100];
    if (scanf("%s", str) == 1) {
        printf("You entered: %s\n", str);
    }

    printf("Enter a hex number: ");
    unsigned int hex;
    if (scanf("%x", &hex) == 1) {
        printf("You entered: 0x%x (%d)\n", hex, hex);
    }

    printf("Enter an octal number: ");
    unsigned int oct;
    if (scanf("%o", &oct) == 1) {
        printf("You entered: 0%o (%d)\n", oct, oct);
    }

    return 0;
}