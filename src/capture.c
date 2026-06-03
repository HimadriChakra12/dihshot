#include "capture.h"
#include "xutil.h"

#include <X11/Xlib.h>

XImage           *img = NULL;
unsigned long int r, g, b;

int screenshot(void) {
    img = XGetImage(
        disp, root,
        0, 0,
        W, H,
        AllPlanes,
        ZPixmap
    );
    if (!img) return 0;

    r = img->red_mask;
    g = img->green_mask;
    b = img->blue_mask;
    return 1;
}
