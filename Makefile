CC     = cc
CFLAGS = -O2 -Wall -Wextra -std=c99 -Isrc
LIBS   = -lX11 -lwebp

# Uncomment to enable debug output
# CFLAGS += -DDEBUG

SRCS = main.c src/xutil.c src/capture.c src/select.c src/save.c src/scripts.c
OBJS = $(SRCS:.c=.o)

screenshot: $(OBJS)
	$(CC) $(OBJS) -o $@ $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f main.o $(patsubst %.c,%.o,$(filter src/%,$(SRCS))) screenshot

.PHONY: clean
