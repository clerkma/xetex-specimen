/* public domain */
#include <stdio.h>
#include <unicode/ucnv.h>

int main(void)
{
    int i = 0;
    int sum = 0;

    sum = ucnv_countAvailable();
    printf("Available encoding name(s) for \\XeTeXinputencoding:\n");
    for (i = 0; i < sum; i++)
    {
        const char *name = ucnv_getAvailableName(i);
        printf(" (%d) -> '%s'\n", i, name);
    }

    return 0;
}