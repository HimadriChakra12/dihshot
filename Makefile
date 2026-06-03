CC      = cc
CFLAGS  = -O2 -Wall -Wextra -std=c99
LIBS    = -lX11 -lwebp

# Uncomment to enable debug output
# CFLAGS += -DDEBUG

SRCS = main.c xutil.c capture.c select.c save.c scripts.c
OBJS = $(SRCS:.c=.o)

screenshot: $(OBJS)
	$(CC) $(OBJS) -o $@ $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) screenshot

.PHONY: clean
