#pragma once

#include <X11/Xlib.h>

extern XImage           *img;
extern unsigned long int r, g, b;

int screenshot(void);
