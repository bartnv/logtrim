logtrunc: main.c
	gcc -o logtrunc -g -Wall -Wextra main.c

install:
	cp logtrunc /usr/local/bin
