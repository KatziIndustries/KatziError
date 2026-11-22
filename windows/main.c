#include <stdio.h>

#include "kerror.h"

int kerror(char *text)
{
    printf("Unsupport please update your implementation\n");
    return 0;
}

void kerror_init()
{
    windows_native_init("");
}

int kerror_pull()
{
    return windows_native_pull();
}

void kerror_close()
{
    windows_native_close();
}

void redraw()
{
}