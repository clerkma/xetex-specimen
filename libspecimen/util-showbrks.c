/* public domain */
#include <stdio.h>
#include <unicode/ubrk.h>

int main(void)
{
    int i = 0;
    int sum = 0;

    sum = ubrk_countAvailable();
    printf("Available locale name(s) for \\XeTeXlinebreaklocale:\n");
    for (i = 0; i < sum; i++)
    {
        const char *name = ubrk_getAvailable(i);
        printf(" (%d) -> '%s'\n", i, name);
    }

    return 0;
}