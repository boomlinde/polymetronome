LDLIBS = -lm $(shell pkg-config --libs sdl2)
CFLAGS = -O3 $(shell pkg-config --cflags sdl2)

polymetronome: polymetronome.o

polymetronome.o: polymetronome.c

clean:
	-rm -f polymetronome *.o
