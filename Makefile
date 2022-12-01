LDLIBS = -lm
CFLAGS = -O3

polymetronome: polymetronome.o

polymetronome.o: polymetronome.c

clean:
	-rm -f polymetronome *.o
