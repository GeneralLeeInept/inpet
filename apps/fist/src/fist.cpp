#include <gli.h>

#include "app.h"

fist::App app;

int gli_main(int argc, char** argv)
{
    if (app.initialize("Fist", 1280, 720, 1))
    {
        app.show_mouse(true);
        app.run();
    }

    return 0;
}
