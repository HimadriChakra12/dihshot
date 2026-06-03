#include "select.h"
#include "xutil.h"
#include "capture.h"
#include "scripts.h"
#include "config.h"

#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/cursorfont.h>
#include <stdlib.h>
#include <string.h>

// ── Rect ─────────────────────────────────────────────────────────────────────
static Rect rect;

int select_x(void) { return rect.x; }
int select_y(void) { return rect.y; }
int select_w(void) { return rect.w; }
int select_h(void) { return rect.h; }

// ── Drag/resize handle logic ──────────────────────────────────────────────────
//
// 9 zones:  TL T TR
//            L  B  R      (B = body / move)
//           BL Bo BR
//
typedef enum {
    ZONE_NONE = 0,
    ZONE_BODY,
    ZONE_TL, ZONE_T, ZONE_TR,
    ZONE_L,           ZONE_R,
    ZONE_BL, ZONE_B,  ZONE_BR,
} Zone;

static Zone hit_zone(int mx, int my) {
    int x2 = rect.x + rect.w;
    int y2 = rect.y + rect.h;
    int hs = OPTHANDLESIZE;

    if (mx < rect.x - hs || mx > x2 + hs ||
        my < rect.y - hs || my > y2 + hs)
        return ZONE_NONE;

    int onL  = mx <= rect.x + hs;
    int onR  = mx >= x2    - hs;
    int onT  = my <= rect.y + hs;
    int onBo = my >= y2    - hs;

    if (onT  && onL)  return ZONE_TL;
    if (onT  && onR)  return ZONE_TR;
    if (onBo && onL)  return ZONE_BL;
    if (onBo && onR)  return ZONE_BR;
    if (onT)          return ZONE_T;
    if (onBo)         return ZONE_B;
    if (onL)          return ZONE_L;
    if (onR)          return ZONE_R;
    return ZONE_BODY;
}

static unsigned int zone_cursor(Zone z) {
    switch (z) {
        case ZONE_TL: case ZONE_BR: return XC_sizing;
        case ZONE_TR: case ZONE_BL: return XC_sizing;
        case ZONE_T:  case ZONE_B:  return XC_sb_v_double_arrow;
        case ZONE_L:  case ZONE_R:  return XC_sb_h_double_arrow;
        case ZONE_BODY:             return XC_fleur;
        default:                    return XC_crosshair;
    }
    return XC_crosshair; /* unreachable */
}

// Apply a mouse delta to rect depending on which zone is being dragged.
static void apply_drag(Zone z, int dx, int dy) {
    switch (z) {
        case ZONE_BODY: rect.x += dx; rect.y += dy; break;
        case ZONE_TL:   rect.x += dx; rect.y += dy;
                        rect.w -= dx; rect.h -= dy; break;
        case ZONE_TR:                 rect.y += dy;
                        rect.w += dx; rect.h -= dy; break;
        case ZONE_BL:   rect.x += dx;
                        rect.w -= dx; rect.h += dy; break;
        case ZONE_BR:   rect.w += dx; rect.h += dy; break;
        case ZONE_T:                  rect.y += dy;
                                      rect.h -= dy; break;
        case ZONE_B:    rect.h += dy;               break;
        case ZONE_L:    rect.x += dx; rect.w -= dx; break;
        case ZONE_R:    rect.w += dx;               break;
        default: break;
    }
    // Keep minimum size
    if (rect.w < 2) rect.w = 2;
    if (rect.h < 2) rect.h = 2;
}

// ── Drawing ───────────────────────────────────────────────────────────────────
static void draw_handle(Pixmap pb, int cx, int cy) {
    int hs = OPTHANDLESIZE;
    XFillRectangle(disp, pb, gc,
                   cx - hs/2, cy - hs/2, hs, hs);
}

static void redraw(void) {
    // Blit screenshot
    XPutImage(disp, backbuffer, gc, img, 0, 0, 0, 0, W, H);

    // Dim outside selection
    XSetForeground(disp, gc, OPTDIMCOLOR);
    // top strip
    XFillRectangle(disp, backbuffer, gc, 0, 0, W, rect.y);
    // bottom strip
    XFillRectangle(disp, backbuffer, gc, 0, rect.y + rect.h, W, H - rect.y - rect.h);
    // left strip
    XFillRectangle(disp, backbuffer, gc, 0, rect.y, rect.x, rect.h);
    // right strip
    XFillRectangle(disp, backbuffer, gc, rect.x + rect.w, rect.y,
                   W - rect.x - rect.w, rect.h);

    // Selection border
    XSetForeground(disp, gc, (OPTR << 16) + (OPTG << 8) + OPTB);
    XDrawRectangle(disp, backbuffer, gc, rect.x, rect.y, rect.w, rect.h);

    // Handles (filled squares at corners + edge midpoints)
    int mx = rect.x + rect.w / 2;
    int my = rect.y + rect.h / 2;
    draw_handle(backbuffer, rect.x,          rect.y);
    draw_handle(backbuffer, mx,               rect.y);
    draw_handle(backbuffer, rect.x + rect.w, rect.y);
    draw_handle(backbuffer, rect.x,          my);
    draw_handle(backbuffer, rect.x + rect.w, my);
    draw_handle(backbuffer, rect.x,          rect.y + rect.h);
    draw_handle(backbuffer, mx,               rect.y + rect.h);
    draw_handle(backbuffer, rect.x + rect.w, rect.y + rect.h);

    // Dimension label  "WxH" above the rect
    char label[32];
    snprintf(label, sizeof(label), "%d x %d", rect.w, rect.h);
    XSetForeground(disp, gc, (OPTR << 16) + (OPTG << 8) + OPTB);
    int ly = rect.y - 6;
    if (ly < 12) ly = rect.y + rect.h + 14;
    XDrawString(disp, backbuffer, gc, rect.x, ly, label, strlen(label));

    XCopyArea(disp, backbuffer, win, gc, 0, 0, W, H, 0, 0);
    XSync(disp, False);
}

// ── Keyboard grab helper ──────────────────────────────────────────────────────
static int grab_keyboard(void) {
    return XGrabKeyboard(
        disp, root, False,
        GrabModeAsync, GrabModeAsync,
        CurrentTime
    ) == GrabSuccess;
}

// ── Window picking ────────────────────────────────────────────────────────────
//
// Walk the window tree to find the deepest mapped window that contains (mx,my)
// that is a direct child of root (i.e. a real top-level / WM-managed window).
// Returns None if nothing suitable found.
//
static Window find_top_level_at(int mx, int my) {
    Window parent, *children = NULL;
    unsigned int nchildren;
    if (!XQueryTree(disp, root, &(Window){0}, &parent, &children, &nchildren))
        return None;

    // Iterate in reverse (topmost windows are last in the stacking order)
    Window found = None;
    for (int i = (int)nchildren - 1; i >= 0; i--) {
        XWindowAttributes wa;
        if (!XGetWindowAttributes(disp, children[i], &wa)) continue;
        if (wa.map_state != IsViewable)                    continue;
        if (mx < wa.x || mx >= wa.x + wa.width)           continue;
        if (my < wa.y || my >= wa.y + wa.height)           continue;
        found = children[i];
        break;
    }
    if (children) XFree(children);
    return found;
}

// Get geometry of a window relative to root.
static int window_rect(Window w, Rect *out) {
    Window child;
    int rx, ry;
    unsigned int rw, rh, bw, depth;
    if (!XGetGeometry(disp, w, &(Window){0}, &rx, &ry, &rw, &rh, &bw, &depth))
        return 0;
    // Translate to root coordinates (XGetGeometry gives parent-relative coords)
    if (!XTranslateCoordinates(disp, w, root, 0, 0, &rx, &ry, &child))
        return 0;
    out->x = rx;
    out->y = ry;
    out->w = (int)rw;
    out->h = (int)rh;
    return 1;
}

// Draw a highlight rect over a window (call before XCopyArea to screen)
static void highlight_window(Window w) {
    Rect wr;
    if (!window_rect(w, &wr)) return;

    XPutImage(disp, backbuffer, gc, img, 0, 0, 0, 0, W, H);

    // Dim everything outside the hovered window
    XSetForeground(disp, gc, OPTDIMCOLOR);
    XFillRectangle(disp, backbuffer, gc, 0, 0, W, wr.y);
    XFillRectangle(disp, backbuffer, gc, 0, wr.y + wr.h, W, H - wr.y - wr.h);
    XFillRectangle(disp, backbuffer, gc, 0, wr.y, wr.x, wr.h);
    XFillRectangle(disp, backbuffer, gc,
                   wr.x + wr.w, wr.y, W - wr.x - wr.w, wr.h);

    // Border
    XSetForeground(disp, gc, (OPTR << 16) + (OPTG << 8) + OPTB);
    XDrawRectangle(disp, backbuffer, gc, wr.x, wr.y, wr.w, wr.h);

    XCopyArea(disp, backbuffer, win, gc, 0, 0, W, H, 0, 0);
    XSync(disp, False);
}

// ── Pre-selection mode chooser ────────────────────────────────────────────────
//
// Called before the pointer is grabbed.  Grabs the keyboard, shows a crosshair,
// and waits for:
//   f        → MODE_FULLSCREEN
//   w        → MODE_WINDOW
//   Escape   → cancel
//   anything else / click → MODE_REGION (default)
//
static Mode pick_mode(void) {
    // Grab keyboard only — pointer is still free so the crosshair cursor is
    // shown by the server default, not our grab cursor.
    if (XGrabKeyboard(disp, root, False,
                      GrabModeAsync, GrabModeAsync,
                      CurrentTime) != GrabSuccess) {
        debug("pre-select keyboard grab failed, defaulting to region");
        return MODE_REGION;
    }

    Mode mode = MODE_REGION; // default if user just clicks
    XEvent e;

    while (1) {
        XNextEvent(disp, &e);

        if (e.type == KeyPress) {
            KeySym ks = XLookupKeysym(&e.xkey, 0);
            if (ks == XK_Escape) {
                mode = (Mode)-1;
                break;
            }
            if (ks == OPTKEY_FULLSCREEN) { mode = MODE_FULLSCREEN; break; }
            if (ks == OPTKEY_WINDOW)     { mode = MODE_WINDOW;     break; }
            // Any other key → region (fall through to pointer grab)
            break;
        }

        // Mouse button press before any key → region mode, keep the event
        if (e.type == ButtonPress) {
            XPutBackEvent(disp, &e);
            break;
        }
    }

    XUngrabKeyboard(disp, CurrentTime);
    return mode;
}

// ── Main selection loop ───────────────────────────────────────────────────────

// Out-params set on SELECT_OK
static int    chosen_script = -1; // -1 = default save, >=0 = scripts[] index
static Script loaded_scripts[MAX_SCRIPTS];
static int    nscripts = 0;

int select_chosen_script(void)        { return chosen_script; }
Script *select_scripts(void)          { return loaded_scripts; }
int    select_nscripts(void)          { return nscripts; }

int run_selection(void) {
    nscripts = scripts_load(loaded_scripts);

    // ── Pre-selection: choose mode before grabbing the pointer ────────────────
    Mode mode = pick_mode();
    if ((int)mode == -1) return SELECT_CANCEL; // Escape in mode picker

    // ── FULLSCREEN: no drawing needed, rect = whole screen ────────────────────
    if (mode == MODE_FULLSCREEN) {
        if (!screenshot()) return SELECT_ERROR;
        rect.x = 0; rect.y = 0; rect.w = W; rect.h = H;
        XMapRaised(disp, win);
        XSync(disp, False);
        // Show the full screen dimmed (nothing to dim) with border, then
        // immediately enter SELECTED state so the user can pick a script.
        if (!grab_keyboard()) debug("Keyboard grab failed — shortcuts disabled");
        redraw();
        debug("MODE_FULLSCREEN: rect=0,0 %dx%d", W, H);
        // Fall through to the SELECTED event loop below.
        goto post_selection;
    }

    // ── Grab pointer now that mode is chosen ──────────────────────────────────
    if (!xutil_grab_pointer()) return SELECT_CANCEL; // already logged by caller

    // ── WINDOW: hover-highlight, click to commit ───────────────────────────────
    if (mode == MODE_WINDOW) {
        if (!screenshot()) return SELECT_ERROR;
        XMapRaised(disp, win);
        XSync(disp, False);

        Window hovered = None;
        XEvent e;

        while (1) {
            XNextEvent(disp, &e);

            if (e.type == MotionNotify) {
                Window w = find_top_level_at(e.xmotion.x, e.xmotion.y);
                if (w != hovered) {
                    hovered = w;
                    if (hovered != None)
                        highlight_window(hovered);
                    else {
                        // Mouse not over any window — just show raw screenshot
                        XPutImage(disp, win, gc, img, 0, 0, 0, 0, W, H);
                        XSync(disp, False);
                    }
                }

            } else if (e.type == ButtonPress) {
                if (e.xbutton.button == Button3) {
                    XUngrabPointer(disp, CurrentTime);
                    return SELECT_CANCEL;
                }
                if (e.xbutton.button == Button1) {
                    hovered = find_top_level_at(e.xbutton.x, e.xbutton.y);
                    if (hovered == None || !window_rect(hovered, &rect)) {
                        XUngrabPointer(disp, CurrentTime);
                        return SELECT_CANCEL;
                    }
                    debug("MODE_WINDOW: picked window 0x%lx  rect=%d,%d %dx%d",
                          hovered, rect.x, rect.y, rect.w, rect.h);
                    break;
                }

            } else if (e.type == KeyPress) {
                KeySym ks = XLookupKeysym(&e.xkey, 0);
                if (ks == XK_Escape) {
                    XUngrabPointer(disp, CurrentTime);
                    return SELECT_CANCEL;
                }
            }
        }

        XUngrabPointer(disp, CurrentTime);
        if (!grab_keyboard()) debug("Keyboard grab failed — shortcuts disabled");
        redraw();
        goto post_selection;
    }

    // ── REGION: drag out a rectangle ─────────────────────────────────────────
    {
        State  state    = STATE_SELECTING;
        Zone   dragging = ZONE_NONE;
        int    px = 0, py = 0;
        int    anchor_x = 0, anchor_y = 0;
        XEvent e;

        // Wait for left-click (right-click cancels)
        while (1) {
            XNextEvent(disp, &e);
            if (e.type == ButtonPress) {
                if (e.xbutton.button == Button3) {
                    XUngrabPointer(disp, CurrentTime);
                    return SELECT_CANCEL;
                }
                if (e.xbutton.button == Button1) break;
            }
            if (e.type == KeyPress) {
                KeySym ks = XLookupKeysym(&e.xkey, 0);
                if (ks == XK_Escape) {
                    XUngrabPointer(disp, CurrentTime);
                    return SELECT_CANCEL;
                }
            }
        }

        anchor_x = rect.x = e.xbutton.x;
        anchor_y = rect.y = e.xbutton.y;
        rect.w = rect.h = 0;
        px = anchor_x; py = anchor_y;

        if (!screenshot()) { XUngrabPointer(disp, CurrentTime); return SELECT_ERROR; }

        XMapRaised(disp, win);
        XSync(disp, False);
        XPutImage(disp, win, gc, img, 0, 0, 0, 0, W, H);
        XSync(disp, False);

        while (state != STATE_DONE) {
            XNextEvent(disp, &e);

            switch (state) {
            case STATE_SELECTING:
                if (e.type == MotionNotify) {
                    int cx = e.xmotion.x, cy = e.xmotion.y;
                    rect.x = anchor_x < cx ? anchor_x : cx;
                    rect.y = anchor_y < cy ? anchor_y : cy;
                    rect.w = abs(cx - anchor_x);
                    rect.h = abs(cy - anchor_y);
                    redraw();

                } else if (e.type == ButtonRelease && e.xbutton.button == Button1) {
                    if (rect.w < 2 || rect.h < 2) {
                        XUngrabPointer(disp, CurrentTime);
                        return SELECT_CANCEL;
                    }
                    state = STATE_SELECTED;
                    XUngrabPointer(disp, CurrentTime);
                    if (!grab_keyboard()) debug("Keyboard grab failed");
                    xutil_set_cursor(XC_fleur);
                    redraw();
                    debug("STATE -> SELECTED  rect=%d,%d %dx%d",
                          rect.x, rect.y, rect.w, rect.h);

                } else if (e.type == KeyPress) {
                    KeySym ks = XLookupKeysym(&e.xkey, 0);
                    if (ks == XK_Escape) {
                        XUngrabPointer(disp, CurrentTime);
                        return SELECT_CANCEL;
                    }
                }
                break;

            case STATE_SELECTED:
                if (e.type == MotionNotify) {
                    int cx = e.xmotion.x, cy = e.xmotion.y;
                    if (dragging != ZONE_NONE) {
                        apply_drag(dragging, cx - px, cy - py);
                        redraw();
                    } else {
                        xutil_set_cursor(zone_cursor(hit_zone(cx, cy)));
                    }
                    px = cx; py = cy;

                } else if (e.type == ButtonPress && e.xbutton.button == Button1) {
                    Zone z = hit_zone(e.xbutton.x, e.xbutton.y);
                    if (z == ZONE_NONE) {
                        // Click outside — re-grab pointer and start over
                        XUngrabKeyboard(disp, CurrentTime);
                        if (!xutil_grab_pointer()) return SELECT_CANCEL;
                        state = STATE_SELECTING;
                        anchor_x = rect.x = e.xbutton.x;
                        anchor_y = rect.y = e.xbutton.y;
                        rect.w = rect.h = 0;
                        px = anchor_x; py = anchor_y;
                        xutil_set_cursor(XC_crosshair);
                        debug("STATE -> SELECTING (re-select)");
                    } else {
                        dragging = z;
                        px = e.xbutton.x;
                        py = e.xbutton.y;
                    }

                } else if (e.type == ButtonRelease && e.xbutton.button == Button1) {
                    dragging = ZONE_NONE;

                } else if (e.type == ButtonPress && e.xbutton.button == Button3) {
                    XUngrabKeyboard(disp, CurrentTime);
                    return SELECT_CANCEL;

                } else if (e.type == KeyPress) {
                    goto handle_key; // shared with window/fullscreen path
                }
                break;

            case STATE_DONE: break;
            }
        }

        XUngrabKeyboard(disp, CurrentTime);
        return SELECT_OK;
    }

post_selection:
    // ── Post-selection keyboard loop (window + fullscreen modes land here) ────
    {
        State state = STATE_SELECTED;
        Zone  dragging = ZONE_NONE;
        int   px = 0, py = 0;
        XEvent e;

        // Also re-grab pointer so drag/resize works in window mode too
        if (!xutil_grab_pointer()) {
            // Non-fatal — keyboard actions still work
            debug("Post-selection pointer grab failed");
        }

        while (state != STATE_DONE) {
            XNextEvent(disp, &e);

            if (e.type == MotionNotify) {
                int cx = e.xmotion.x, cy = e.xmotion.y;
                if (dragging != ZONE_NONE) {
                    apply_drag(dragging, cx - px, cy - py);
                    redraw();
                } else {
                    xutil_set_cursor(zone_cursor(hit_zone(cx, cy)));
                }
                px = cx; py = cy;

            } else if (e.type == ButtonPress && e.xbutton.button == Button1) {
                Zone z = hit_zone(e.xbutton.x, e.xbutton.y);
                if (z != ZONE_NONE) {
                    dragging = z;
                    px = e.xbutton.x;
                    py = e.xbutton.y;
                }

            } else if (e.type == ButtonRelease && e.xbutton.button == Button1) {
                dragging = ZONE_NONE;

            } else if (e.type == ButtonPress && e.xbutton.button == Button3) {
                XUngrabPointer(disp, CurrentTime);
                XUngrabKeyboard(disp, CurrentTime);
                return SELECT_CANCEL;

            } else if (e.type == KeyPress) {
                (void)state; /* suppress maybe-unused */
                handle_key: {
                    KeySym ks = XLookupKeysym(&e.xkey, 0);
                    if (ks == XK_Escape) {
                        XUngrabPointer(disp, CurrentTime);
                        XUngrabKeyboard(disp, CurrentTime);
                        return SELECT_CANCEL;
                    }
                    if (ks == XK_Return || ks == XK_KP_Enter || ks == XK_space) {
                        chosen_script = -1; state = STATE_DONE;
                        debug("STATE -> DONE (default save)");
                        break;
                    }
                    if (ks >= XK_1 && ks <= XK_9) {
                        int idx = (int)(ks - XK_1);
                        if (idx < nscripts) {
                            chosen_script = idx; state = STATE_DONE;
                            debug("STATE -> DONE (script %d: %s)",
                                  idx, loaded_scripts[idx].name);
                        }
                        break;
                    }
                    #ifdef OPTKEYBINDS
                    {
                        static const struct { KeySym sym; int script_idx; }
                            kb[] = OPTKEYBINDS;
                        for (size_t i = 0; i < sizeof(kb)/sizeof(kb[0]); i++) {
                            if (ks == kb[i].sym) {
                                chosen_script = kb[i].script_idx;
                                state = STATE_DONE;
                                break;
                            }
                        }
                    }
                    #endif
                }
            }
        }

        XUngrabPointer(disp, CurrentTime);
        XUngrabKeyboard(disp, CurrentTime);
        return SELECT_OK;
    }
}
