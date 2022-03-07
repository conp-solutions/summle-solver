summle: main.c Makefile
	gcc main.c -o summle -Wall -Wextra -g -O0

clean:
	rm -f summle
