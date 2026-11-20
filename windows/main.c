#include <stdio.h>

#include "kerror.h"

int kerror(char *text)
{
    return windows_native(text);
}