#include <stdbool.h>

#include "kerror.h"

Message msg = {
    .title = "Message 1",
    .text = "Error occurred",
    .buttoncount = 1,
    .button1 = "Ok",
    .button2 = "...",
    .progressactive = false,
    .progress = 1.0f
};

void kerror_set_title(char *t)
{
    msg.title = t;
    redraw();
}

void kerror_set_text(char *t)
{
    msg.text = t;
    redraw();
}

void kerror_set_buttoncount(int n)
{
    msg.buttoncount = n;
    redraw();
}

void kerror_set_button1(char *t)
{
    msg.button1 = t;
    redraw();
}

void kerror_set_button2(char *t)
{
    msg.button2 = t;
    redraw();
}

void kerror_set_progressactive(bool b)
{
    msg.progressactive = b;
    redraw();
}

void kerror_set_progress(float f)
{
    msg.progress = f;
    redraw();
}