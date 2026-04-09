// Config

#define OPTDIR "~/Pictures/screenshots/"
#define OPTFORMAT "%d-%02d-%02d_%02d:%02d:%02d.webp"
#define OPTFORMATARGS tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec
#define OPTQUALITY 80
#define OPTWIDTH 1
#define OPTR 255
#define OPTG 255
#define OPTB 255

// Include

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/cursorfont.h>

#include <webp/encode.h>

#include <unistd.h>
#include <pwd.h>

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/stat.h>
#include <sys/types.h>

#include <linux/limits.h>

// Statics

static Display* disp;
static int      scrn;
static Window   root;

static unsigned long int r, g, b;
static Time lasttime;
static Window win = 0;
static XEvent e;
static XImage* img = NULL;
static GC gc = NULL;
static Pixmap backbuffer = 0;
static XGCValues gcv;

static XWindowAttributes _xwa;
#define W _xwa.width
#define H _xwa.height

#define die(...) { printf("\033[1;31mError:\033[0m "); printf(__VA_ARGS__); putchar('\n'); goto end; }
#ifdef DEBUG
	#define debug(...) { printf("\033[1;33mDebug:\033[0m "); printf(__VA_ARGS__); putchar('\n'); }
#else
	#define debug(...) {}
#endif

static XPoint rectpts[] = { // pts used to draw rect
//    X  Y
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
};
#define rectx rectpts[0].x
#define recty rectpts[0].y
static int rectw;
static int recth;

//

static inline int screenshot() {
	// Get Screen
	img = XGetImage(
		disp, root, 
		0, 0,
		W, H,
		AllPlanes,
		ZPixmap
	);
	if (img == NULL) return 0;
	r = img->red_mask;
	g = img->green_mask;
	b = img->blue_mask;
	return 1;
}

int main(int argc, char* argv[]) {
	(void)argv;
	(void)argc;

	// Init X
	disp = XOpenDisplay(NULL);
	if (disp == NULL) die("Failed to open display");
	scrn = XDefaultScreen(disp);
	root = XRootWindow(disp, scrn);
	if (XGetWindowAttributes(disp, root, &_xwa) == 0) // allow use of W, H (screen size)
		die("Failed to get root window size");

	// Arg parse
	if (argc > 1) { // If there's an arg, take a full screenshot
		if (!screenshot()) die("Failed to get screenshot image");
		goto noselection;
	}

	// Override redirect flag
	static XSetWindowAttributes attrs;
	static unsigned long attrsmask = 0;
	attrs.override_redirect = True;
	attrsmask |= CWOverrideRedirect;

	// Create Window
	win = XCreateWindow(
		disp, root, 
		0, 0, W, H, 
		0, CopyFromParent,
		InputOutput, CopyFromParent,
		attrsmask, &attrs
	);
	if (!win) die("Failed to create window");

	// Store name
	XStoreName(disp, win, "screenshot");
	// Transient for (root)
	XSetTransientForHint(disp, win, root);
	// Dialog type window
	Atom wm_window_type = XInternAtom(disp, "_NET_WM_WINDOW_TYPE", False);
	Atom wm_window_type_dialog = XInternAtom(disp, "_NET_WM_WINDOW_TYPE_DIALOG", False);
	XChangeProperty(
		disp, win, wm_window_type, XA_ATOM, 32,
		PropModeReplace, (unsigned char *)&wm_window_type_dialog, 1
	);
	// Get exposure events
	XSelectInput(disp, win, ExposureMask);

	// Create GC
	gcv.function = 0;
	gcv.line_width = OPTWIDTH;
	gc = XCreateGC(disp, win, GCLineWidth, &gcv);
	XSetForeground(disp, gc, (OPTR << 16) + (OPTG << 8) + OPTB);

	// Create backbuffer
	backbuffer = XCreatePixmap(disp, win, W, H, 24);

	// Grab Pointer
	if (XGrabPointer(
		disp, root,
		False, ButtonPressMask|ButtonReleaseMask|PointerMotionMask, 
		GrabModeAsync, GrabModeAsync,
		None, XCreateFontCursor(disp, XC_sizing), 
		CurrentTime
	) != GrabSuccess) die("Failed to grab mouse");

	// Wait for first Button Press
	do {
		XNextEvent(disp, &e);
	} while (e.type != ButtonPress);
	if ((((XButtonEvent*)&e)->button) != Button1) goto end;
	rectx = e.xbutton.x;
	recty = e.xbutton.y;
	rectw = 0;
	recth = 0;

	if (!screenshot()) die("Failed to get screenshot image");

	// Raise/Map Window
	XMapRaised(disp, win);
	XSync(disp, False);

	debug("Starting selection");
	
	// Main loop
	XPutImage(disp, win, gc, img, 0, 0, 0, 0, W, H);
	do {
		XNextEvent(disp, &e);
		if (e.type == MotionNotify) {
			rectw = e.xbutton.x - rectx;
			recth = e.xbutton.y - recty;
			rectpts[1].x =  rectw;
			rectpts[2].y =  recth;
			rectpts[3].x = -rectw;
			rectpts[4].y = -recth;
			debug("@ %d %d # %d %d", rectx, recty, rectw, recth);
			lasttime = e.xmotion.time;
			XPutImage(disp, backbuffer, gc, img, 0, 0, 0, 0, W, H);
			XDrawLines(disp, backbuffer, gc, rectpts, 5, CoordModePrevious);
			XCopyArea(disp, backbuffer, win, gc, 0, 0, W, H, 0, 0);
			XSync(disp, False);
		}
	} while (e.type != ButtonRelease);

	// Fix coords
	if (rectw == 0 || recth == 0) goto end; // Can't take a screenshot of nothing ey?
	if (rectw < 0) {
		rectx += rectw;
		rectw *= -1;
	}
	if (recth < 0) {
		recty += recth;
		recth *= -1;
	}

	// Take subimage
	img = XSubImage(img, rectx, recty, rectw, recth);
	noselection:
	XSync(disp, False); // Wait for screenshot to be processed / cropped

	// Change xxxA to xxx (yay array manipulation)
	char *p1 = img->data + 3;
	char *p2 = img->data + 4;
	while (p2 - img->data < img->height * img->bytes_per_line) {
		*p1 = *p2;
		++p1; ++p2;
		*p1 = *p2;
		++p1; ++p2;
		*p1 = *p2;
		++p1;
		p2 += 2;
	}
	/*
		// This is equal to the code above but less effecient
		for (int i = 3, j = 4; j < img->height * img->bytes_per_line; i += 3, j += 4) {
			img->data[i] = img->data[j];
			img->data[i + 1] = img->data[j + 1];
			img->data[i + 2] = img->data[j + 2];
		}
	*/

	// Save file
	static FILE* fp = NULL;
	char fn[PATH_MAX];
	
	time_t t = time(NULL);
	struct tm tm = *localtime(&t);

	#ifdef OPTDIR
		if (OPTDIR[0] == '~') {
			const char* home = getenv("HOME");
			if (!home) {
				const struct passwd *pw = getpwuid(getuid());
				home = pw->pw_dir;
			}
			if (!home)
				die("Couldn't resolve tilde in OPTDIR");
			strncpy(fn + strlen(fn), home, sizeof(fn) - strlen(fn));
			strncpy(fn + strlen(fn), &OPTDIR[1], sizeof(fn) - strlen(fn));
		} else {
			strncpy(fn + strlen(fn), OPTDIR, sizeof(fn) - strlen(fn));
		}
	#else
		fnptr = strncpy(fn + strlen(fn), "/tmp/", sizeof(path) - strlen(fn));
	#endif

	mkdir(fn, 0755);
	snprintf(fn + strlen(fn), sizeof(fn) - strlen(fn), OPTFORMAT, OPTFORMATARGS);

	fp = fopen(fn, "wb");
	if (fp == NULL) die("Can't open %s", fn);
	debug("Writing to %s", fn);

	static unsigned char* fd = NULL;
	static size_t fs = 0;
	if (r < g && g < b) { // RGB
		fs = WebPEncodeRGB((unsigned char *)img->data, img->width, img->height, img->bytes_per_line * 3 / 4, OPTQUALITY, &fd);
		debug("Pixel format = RGB");
	} else if (b < g && g < r) { // BGR
		fs = WebPEncodeBGR((unsigned char *)img->data, img->width, img->height, img->bytes_per_line * 3 / 4, OPTQUALITY, &fd);
		debug("Pixel format = BGR");
	} else {
		debug("Unsupported pixel format %lu %lu %lu", r, g, b);
		die("Unsupported pixel format");
	}
	debug(
		"Image size %dx%d %d bytes (%d bytes/line) -> %dx%d %ld bytes (%d bytes/line)", 
		img->width, img->height, img->width * img->height, img->bytes_per_line,
		img->width, img->height, fs, img->bytes_per_line * 3 / 4
	);
	if (fs <= 0) die("Failed to encode webp");

	static size_t written = 0;
	written = fwrite(fd, 1, fs, fp);
	if (!written) die("Failed to write to %s", fn);
	if (written != fs) die("Failed to fully write to %s, wrote %lu / %lu bytes", fn, written, fs);

	static int failed = 1;
	failed = 0;
	end:

		XUngrabPointer(disp, CurrentTime);
		if (fd) free(fd);
		if (fp) fclose(fp);
		if (img) XDestroyImage(img);
		if (backbuffer) XFreePixmap(disp, backbuffer);
		if (gc) XFreeGC(disp, gc);
		if (win) XDestroyWindow(disp, win);
		if (disp) XCloseDisplay(disp);

		if (!failed) {
			printf("%s\n", fn);
			// Copy to clipboard (for some reason a mime type of image/webp doesn't work for some applications)
			char* args[] = {"xclip", "-selection", "clipboard", "-t", "image/png", fn, NULL};
			execvp(args[0], args);
		}
	
	return 0;
}
