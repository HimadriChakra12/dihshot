NAME = screenshot
PREFIX= /usr/local

CC = cc
LIBS = -lX11 -lwebp
FLAGS = -pedantic -Wall
FLAGSDEBUG = -g -fsanitize=address -fno-omit-frame-pointer -O0
FLAGSRELEASE = -O4

build:
	gcc *.c -o $(NAME) $(FLAGS) $(FLAGSRELEASE) $(LIBS)

debug:
	gcc -DDEBUG *.c -o $(NAME) $(FLAGS) $(FLAGSDEBUG) $(LIBS)

install:
	cp "$(NAME)" "$(PREFIX)/bin/$(NAME)"

uninstall:
	rm -f "$(PREFIX)/bin/$(NAME)"
