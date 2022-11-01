all: lesson1

lesson1: lesson1.c
	gcc -Wall lesson1.c $(pkg-config --cflags --libs libpipewire-0.3)

run: a.out
	./a.out
