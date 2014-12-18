# Remove -D__MACOSX_CORE__ if you're not on OS X
CC=gcc -g -D__MACOSX_CORE__ -Wno-deprecated
FLAGS=-c -Wall
LIBS=-framework OpenGL -framework GLUT -lportaudio Biquad.c

OBJS=riser_generator.o

EXE=riser_generator

all: $(OBJS)
	$(CC) -o $(EXE) $(OBJS) $(LIBS)

clean:
	rm -f *~ *.o $(EXE)
