#include "xutil.h"
#include "config.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/cursorfont.h>

Display *disp = NULL;
int      scrn;
Window   root;
Window   win  = 0;
GC       gc   = NULL;
Pixmap   backbuffer = 0;

XWindowAttributes _xwa;

int xutil_init(void) {
    disp = XOpenDisplay(NULL);
    if (!disp) return 0;

    scrn = XDefaultScreen(disp);
    root = XRootWindow(disp, scrn);

    return XGetWindowAttributes(disp, root, &_xwa) != 0;
}

int xutil_create_window(void) {
    static XSetWindowAttributes attrs;
    unsigned long attrsmask = CWOverrideRedirect;
    attrs.override_redirect = True;

    win = XCreateWindow(
        disp, root,
        0, 0, W, H,
        0, CopyFromParent,
        InputOutput, CopyFromParent,
        attrsmask, &attrs
    );
    if (!win) return 0;

    XStoreName(disp, win, "screenshot");
    XSetTransientForHint(disp, win, root);

    Atom wm_window_type        = XInternAtom(disp, "_NET_WM_WINDOW_TYPE",        False);
    Atom wm_window_type_dialog = XInternAtom(disp, "_NET_WM_WINDOW_TYPE_DIALOG", False);
    XChangeProperty(
        disp, win, wm_window_type, XA_ATOM, 32,
        PropModeReplace, (unsigned char *)&wm_window_type_dialog, 1
    );

    XSelectInput(disp, win,
        ExposureMask | KeyPressMask | ButtonPressMask |
        ButtonReleaseMask | PointerMotionMask);
    return 1;
}

int xutil_create_gc(void) {
    XGCValues gcv;
    gcv.function   = 0;
    gcv.line_width = OPTWIDTH;
    gc = XCreateGC(disp, win, GCLineWidth, &gcv);
    if (!gc) return 0;

    XSetForeground(disp, gc, (OPTR << 16) + (OPTG << 8) + OPTB);
    return 1;
}

int xutil_create_backbuffer(void) {
    backbuffer = XCreatePixmap(disp, win, W, H, 24);
    return backbuffer != 0;
}

int xutil_grab_pointer(void) {
    return XGrabPointer(
        disp, root,
        False,
        ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
        GrabModeAsync, GrabModeAsync,
        None, XCreateFontCursor(disp, XC_sizing),
        CurrentTime
    ) == GrabSuccess;
}

void xutil_set_cursor(unsigned int shape) {
    Cursor cur = XCreateFontCursor(disp, shape);
    XChangeActivePointerGrab(
        disp,
        ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
        cur, CurrentTime
    );
    XFreeCursor(disp, cur);
}

void xutil_cleanup(void) {
    XUngrabPointer(disp, CurrentTime);
    if (backbuffer) XFreePixmap(disp, backbuffer);
    if (gc)         XFreeGC(disp, gc);
    if (win)        XDestroyWindow(disp, win);
    if (disp)       XCloseDisplay(disp);
}
