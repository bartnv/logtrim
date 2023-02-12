logtrim: main.c
	gcc -o logtrim -g -Wall -Wextra main.c

install:
	cp logtrim /usr/local/bin
