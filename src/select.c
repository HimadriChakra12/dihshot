#include "select.h"
#include "xutil.h"
#include "capture.h"
#include "scripts.h"
#include "../config.h"

#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/cursorfont.h>
#include <stdlib.h>
#include <string.h>

// ── Rect ─────────────────────────────────────────────────────────────────────
static Rect rect;

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
    // Clamp to screen bounds
    if (rect.x < 0) { rect.w += rect.x; rect.x = 0; }
    if (rect.y < 0) { rect.h += rect.y; rect.y = 0; }
    if (rect.x + rect.w > W) rect.w = W - rect.x;
    if (rect.y + rect.h > H) rect.h = H - rect.y;
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
    XFlush(disp);
}

// ── Keyboard grab helper ──────────────────────────────────────────────────────
//
// GrabModeSync freezes the keyboard device — events queue up in the server
// and are not delivered to any other client until we call XAllowEvents.
// This prevents keypresses from leaking through to the window underneath
// (e.g. pressing 'f' triggering Firefox's find bar).
//
static int grab_keyboard(void) {
    return XGrabKeyboard(
        disp, win, False,
        GrabModeAsync,
        GrabModeSync,
        CurrentTime
    ) == GrabSuccess;
}

// Call after consuming a key event to unfreeze the keyboard for the next one.
static void kbd_allow(void) {
    XAllowEvents(disp, AsyncKeyboard, CurrentTime);
}

// ── Pre-selection mode chooser ────────────────────────────────────────────────
//
// Grabs keyboard, waits for one of the mode keys or Escape.
// A mouse press before any key also selects region mode (event put back).
//
static Mode pick_mode(void) {
    // Grab pointer immediately so a plain click goes straight to region.
    if (!xutil_grab_pointer()) {
        debug("pre-select pointer grab failed");
        return (Mode)-1;
    }
    // Grab keyboard on our overlay window (already mapped + focused).
    // Grabbing on root fails when another window has focus; grabbing on win
    // after XSetInputFocus ensures we own all key events exclusively.
    int have_kbd = XGrabKeyboard(disp, win, False,
                                 GrabModeAsync, GrabModeSync,
                                 CurrentTime) == GrabSuccess;
    if (!have_kbd)
        debug("pre-select keyboard grab failed — f key unavailable");

    Mode mode   = MODE_REGION;
    int  cancel = 0;
    XEvent e;

    while (1) {
        XNextEvent(disp, &e);

        if (e.type == KeyPress) {
            KeySym ks = XLookupKeysym(&e.xkey, 0);
            if (have_kbd) XAllowEvents(disp, AsyncKeyboard, CurrentTime);
            if      (ks == XK_Escape)        { cancel = 1;             break; }
            else if (ks == OPTKEY_FULLSCREEN) { mode = MODE_FULLSCREEN; break; }
            else                             { mode = MODE_REGION;     break; }

        } else if (e.type == ButtonPress) {
            if (e.xbutton.button == Button3) { cancel = 1; break; }
            XPutBackEvent(disp, &e); // replay for region loop
            break;
        }
    }

    if (have_kbd) XUngrabKeyboard(disp, CurrentTime);

    if (cancel) {
        XUngrabPointer(disp, CurrentTime);
        return (Mode)-1;
    }

    // Fullscreen doesn't need the pointer grab — release it
    if (mode == MODE_FULLSCREEN)
        XUngrabPointer(disp, CurrentTime);

    // Region keeps the grab; run_selection will release it after first click
    return mode;
}

// ── Main selection loop ───────────────────────────────────────────────────────

static Action chosen_action     = ACTION_NONE;
static int    chosen_script_idx = -1;
static Script loaded_scripts[MAX_SCRIPTS];
static int    nscripts = 0;

int     select_x(void)           { return rect.x; }
int     select_y(void)           { return rect.y; }
int     select_w(void)           { return rect.w; }
int     select_h(void)           { return rect.h; }
Action  select_action(void)      { return chosen_action; }
int     select_script_idx(void)  { return chosen_script_idx; }
Script *select_scripts(void)     { return loaded_scripts; }
int     select_nscripts(void)    { return nscripts; }

int run_selection(void) {
    nscripts = scripts_load(loaded_scripts);

    // ── Capture screen and raise overlay FIRST ────────────────────────────────
    // This must happen before any user input so that by the time we read keys
    // or mouse clicks, the overlay is already covering all other windows.
    // Browser extensions (Vimium etc.) can no longer intercept our keys once
    // the overlay window has focus.
    if (!screenshot()) return SELECT_ERROR;

    XMapRaised(disp, win);
    XSetInputFocus(disp, win, RevertToPointerRoot, CurrentTime);
    XSync(disp, False);

    // Show the frozen screenshot immediately so the screen doesn't flicker
    XPutImage(disp, win, gc, img, 0, 0, 0, 0, W, H);
    XFlush(disp);

    // ── Pre-selection: choose mode ────────────────────────────────────────────
    Mode mode = pick_mode();
    if ((int)mode == -1) return SELECT_CANCEL;

    // ── FULLSCREEN ────────────────────────────────────────────────────────────
    if (mode == MODE_FULLSCREEN) {
        rect.x = 0; rect.y = 0; rect.w = W; rect.h = H;
        if (!grab_keyboard()) debug("Keyboard grab failed — shortcuts disabled");
        redraw();
        debug("MODE_FULLSCREEN: rect=0,0 %dx%d", W, H);
        goto post_selection;
    }

    // ── REGION: drag out a rectangle ─────────────────────────────────────────
    // Pointer is already grabbed by pick_mode for region mode.
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

        while (state != STATE_DONE) {
            XNextEvent(disp, &e);

            switch (state) {
            case STATE_SELECTING:
                if (e.type == MotionNotify) {
                    // Coalesce: skip to the last queued MotionNotify
                    while (XPending(disp)) {
                        XEvent next;
                        XPeekEvent(disp, &next);
                        if (next.type != MotionNotify) break;
                        XNextEvent(disp, &e);
                    }
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
                    // Coalesce: skip to the last queued MotionNotify
                    while (XPending(disp)) {
                        XEvent next;
                        XPeekEvent(disp, &next);
                        if (next.type != MotionNotify) break;
                        XNextEvent(disp, &e);
                    }
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
                // Coalesce: skip to the last queued MotionNotify
                while (XPending(disp)) {
                    XEvent next;
                    XPeekEvent(disp, &next);
                    if (next.type != MotionNotify) break;
                    XNextEvent(disp, &e);
                }
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
                    kbd_allow(); // unfreeze keyboard — must call on every KeyPress

                    if (ks == XK_Escape) {
                        XUngrabPointer(disp, CurrentTime);
                        XUngrabKeyboard(disp, CurrentTime);
                        return SELECT_CANCEL;
                    }

                    // ── Built-in actions ──────────────────────────────────────
                    if (ks == OPTKEY_SAVE) {
                        chosen_action = ACTION_SAVE;
                        state = STATE_DONE;
                        debug("ACTION_SAVE");
                        break;
                    }
                    if (ks == OPTKEY_COPY) {
                        chosen_action = ACTION_COPY;
                        state = STATE_DONE;
                        debug("ACTION_COPY");
                        break;
                    }
                    if (ks == OPTKEY_ANNOTATE) {
                        chosen_action = ACTION_ANNOTATE;
                        state = STATE_DONE;
                        debug("ACTION_ANNOTATE");
                        break;
                    }

                    // ── Scripts by index (1-9) ────────────────────────────────
                    if (ks >= XK_1 && ks <= XK_9) {
                        int idx = (int)(ks - XK_1);
                        if (idx < nscripts) {
                            chosen_action     = ACTION_SCRIPT;
                            chosen_script_idx = idx;
                            state = STATE_DONE;
                            debug("ACTION_SCRIPT idx=%d (%s)",
                                  idx, loaded_scripts[idx].name);
                        }
                        break;
                    }

                    // ── Named script keybinds from config ─────────────────────
                    #ifdef OPTSCRIPTBINDS
                    {
                        static const struct { KeySym sym; const char *name; }
                            kb[] = OPTSCRIPTBINDS;
                        for (size_t i = 0; i < sizeof(kb)/sizeof(kb[0]); i++) {
                            if (ks != kb[i].sym) continue;
                            for (int j = 0; j < nscripts; j++) {
                                if (strcmp(loaded_scripts[j].name, kb[i].name) != 0)
                                    continue;
                                chosen_action     = ACTION_SCRIPT;
                                chosen_script_idx = j;
                                state = STATE_DONE;
                                debug("ACTION_SCRIPT name=%s idx=%d",
                                      kb[i].name, j);
                                break;
                            }
                            break;
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
