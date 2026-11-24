#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>

#include "../include/kerror.h"

int main() {
    kerror_init();

    kerror_set_title("Hallo Aarono");
    kerror_set_button2("1234567890123");
    kerror_set_buttoncount(2);

    int running = 1;
    int state = 0;

    float progress = 0.0f;

    while (running)
    {
        int r = kerror_pull();

        kerror_set_progress(progress);

        if (r != 0) {
            if (state == 0) {
                kerror_set_title("JRB");
                kerror_set_text("JRB");
                kerror_set_progressactive(true);
            } else {
                running = 0;
            }

            state++;
        }

        if(state == 1){
            if(progress >= 1.0f) {
                kerror_set_title("Just accept it bro");
                kerror_set_text("trust JRB");
                kerror_set_progressactive(false);
                kerror_set_buttoncount(1);
                kerror_set_button1("OK");
                state++;
            } else {
                progress += 0.01f;
            }
        }

        usleep(16000);
    }

    kerror_close();
}