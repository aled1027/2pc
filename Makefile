
# -w to ignore warnings
# getting warnings about redefining block -- 
# previous definitions of typedef __m128i block;
# turned off warning with -Wno-typedef-redefinition, for now.

SRCDIR = src
OBJDIR = obj
OBJFULL = obj/*.o

JUSTGARBLE = JustGarble

CC=clang
CFLAGS= -g -Iinc -I$(JUSTGARBLE)/include -Wno-typedef-redefinition -Wno-unused-function
CFLAGS+= -Wno-unused-variable

LIBS=-lmsgpack -march=native -maes -msse4 -lm -lrt -lcrypto -lssl -lgmp -ljansson

SOURCES := $(wildcard $(SRCDIR)/*.c)
OBJECTS := $(wildcard $(SRCDIR)/*.o)

JUSTGARBLESRC := $(wildcard $(JUSTGARBLE)/src/*.c)

all: $(OBJECTS)
	$(CC) $(JUSTGARBLESRC) $(SOURCES) $(CFLAGS) $(LIBS) 

run_garb_off:
	./a.out garb_offline

run_eval_off:
	./a.out eval_offline

run_eval:
	./a.out eval_online

run_garb:
	./a.out garb_online

make run_tests: 
	./a.out tests

valgrind: 
	make;
	rm valg.out;
	valgrind -v --leak-check=full --show-leak-kinds=all --track-origins=yes --leak-check=yes --log-file=valg.out ./a.out;

gdb_garb:
	gdb --args a.out garb_online

gdb_eval:
	gdb --args a.out eval_online



# -q
# -v

.PHONEY: clean
clean:
	rm garbler evaluator a.out


