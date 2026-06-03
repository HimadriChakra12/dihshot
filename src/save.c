#include "save.h"
#include "capture.h"
#include "../config.h"

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

// ── Internal helpers ──────────────────────────────────────────────────────────

static void strip_alpha_channel(void) {
    char *p1 = img->data + 3;
    char *p2 = img->data + 4;
    while (p2 - img->data < img->height * img->bytes_per_line) {
        *p1 = *p2; ++p1; ++p2;
        *p1 = *p2; ++p1; ++p2;
        *p1 = *p2; ++p1;
        p2 += 2;
    }
}

static int build_path(char fn[PATH_MAX]) {
    fn[0] = '\0';
#ifdef OPTDIR
    if (OPTDIR[0] == '~') {
        const char *home = getenv("HOME");
        if (!home) {
            const struct passwd *pw = getpwuid(getuid());
            if (pw) home = pw->pw_dir;
        }
        if (!home) return 0;
        strncat(fn, home,       PATH_MAX - strlen(fn) - 1);
        strncat(fn, &OPTDIR[1], PATH_MAX - strlen(fn) - 1);
    } else {
        strncat(fn, OPTDIR, PATH_MAX - strlen(fn) - 1);
    }
#else
    strncat(fn, "/tmp/", PATH_MAX - strlen(fn) - 1);
#endif
    mkdir(fn, 0755);
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    snprintf(fn + strlen(fn), PATH_MAX - strlen(fn), OPTFORMAT, OPTFORMATARGS);
    return 1;
}

static int encode_and_write(const char *fn) {
    FILE *fp = fopen(fn, "wb");
    if (!fp) {
        printf("\033[1;31mError:\033[0m Can't open %s\n", fn);
        return 1;
    }
    debug("Writing to %s", fn);

    unsigned char *fd = NULL;
    size_t         fs = 0;

    if (r < g && g < b) {
        fs = WebPEncodeRGB((unsigned char *)img->data,
                           img->width, img->height,
                           img->bytes_per_line * 3 / 4, OPTQUALITY, &fd);
        debug("Pixel format = RGB");
    } else if (b < g && g < r) {
        fs = WebPEncodeBGR((unsigned char *)img->data,
                           img->width, img->height,
                           img->bytes_per_line * 3 / 4, OPTQUALITY, &fd);
        debug("Pixel format = BGR");
    } else {
        fclose(fp);
        printf("\033[1;31mError:\033[0m Unsupported pixel format\n");
        return 1;
    }

    int ret = 1;
    if (fs > 0) {
        size_t written = fwrite(fd, 1, fs, fp);
        if (written == fs) ret = 0;
        else printf("\033[1;31mError:\033[0m Wrote %zu/%zu bytes\n", written, fs);
    } else {
        printf("\033[1;31mError:\033[0m WebP encode failed\n");
    }

    if (fd) free(fd);
    fclose(fp);
    return ret;
}

// ── Public API ────────────────────────────────────────────────────────────────

// Save img to disk; copy the resulting path into out_path (size out_size).
// Returns 0 on success.
int save_image_path(char *out_path, size_t out_size) {
    strip_alpha_channel();

    char fn[PATH_MAX];
    if (!build_path(fn)) {
        printf("\033[1;31mError:\033[0m Couldn't resolve output path\n");
        return 1;
    }
    if (encode_and_write(fn) != 0) return 1;

    strncpy(out_path, fn, out_size - 1);
    out_path[out_size - 1] = '\0';
    return 0;
}

// Legacy: save and exec xclip inline (used by full-screen mode).
int save_image(void) {
    char fn[PATH_MAX];
    if (save_image_path(fn, sizeof(fn)) != 0) return 1;

    printf("%s\n", fn);
    char *args[] = { "xclip", "-selection", "clipboard", "-t", "image/png", fn, NULL };
    execvp(args[0], args);
    return 1; // execvp returned — xclip missing, but file is saved
}
