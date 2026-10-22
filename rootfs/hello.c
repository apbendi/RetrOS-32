/* An example C program. Compile and run it on RetrOS with TinyCC:
 *
 *   exec /bin/tcc.o hello.c -o hello
 *   exec hello
 *
 * or compile and run it in one go:
 *
 *   exec /bin/tcc.o -run hello.c
 */
#include <stdio.h>

int main(int argc, char* argv[])
{
    printf("Hello, world! This is %s, with %d argument%s.\n", argv[0], argc - 1, argc == 2 ? "" : "s");
    for (int i = 1; i < argc; i++) {
        printf("  %d: %s\n", i, argv[i]);
    }
    return 0;
}
