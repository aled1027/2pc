
SRCDIR = src
OBJDIR = obj
OBJFULL = obj/*.o

JUSTGARBLE = JustGarble

CC=clang
CFLAGS= -g -Iinc -I$(JUSTGARBLE)/include
LIBS=-lmsgpack -march=native -maes -msse4 -lm -lrt -lcrypto -lssl -lgmp

SOURCES := $(wildcard $(SRCDIR)/*.c)
OBJECTS := $(wildcard $(SRCDIR)/*.o)

JUSTGARBLESRC := $(wildcard $(JUSTGARBLE)/src/*.c)

all: $(OBJECTS)
	$(CC) $(JUSTGARBLESRC) $(SOURCES) $(CFLAGS) $(LIBS)

.PHONEY: clean
clean:
