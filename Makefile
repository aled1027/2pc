
# -w to ignore warnings
# getting warnings about redefining block -- 
# previous definitions of typedef __m128i block;
# turned off warning with -Wno-typedef-redefinition, for now.

SRCDIR = src
OBJDIR = obj
OBJFULL = obj/*.o

JUSTGARBLE = JustGarble

CC=clang
CFLAGS= -Iinc -I$(JUSTGARBLE)/include -Wno-typedef-redefinition -Wno-unused-function
CFLAGS+= -g

LIBS=-lmsgpack -march=native -maes -msse4 -lm -lcrypto -lssl -lgmp -ljansson

SOURCES := $(wildcard $(SRCDIR)/*.c)
OBJECTS := $(wildcard $(SRCDIR)/*.o)

JUSTGARBLESRC := $(wildcard $(JUSTGARBLE)/src/*.c)

all: 
	$(OBJECTS) $(CC) $(JUSTGARBLESRC) $(SOURCES) $(CFLAGS) $(LIBS) 

gdb_garb_off:
	gdb --args a.out garb_offline

run_garb_off:
	./a.out garb_offline

run_eval_off:
	./a.out eval_offline

run_eval:
	./a.out eval_online

run_garb:
	./a.out garb_online

run_tests: 
	./a.out tests

gdb_tests:
	gdb --args a.out tests

valgrind: 
	make;
	rm valg.out;
	valgrind -v --leak-check=full --show-leak-kinds=all --track-origins=yes --leak-check=yes --log-file=valg.out ./a.out garb_online;

gdb_garb:
	gdb --args a.out garb_online

gdb_eval:
	gdb --args a.out eval_online

# -q
# -v

.PHONEY: clean
clean:
	#rm garbler evaluator a.out; 
	rm -rf files/evaluator_gcs; 
	rm -rf files/garbler_gcs;
	mkdir files/evaluator_gcs;
	mkdir files/garbler_gcs;

# ./a.out garb_online > garb_stdout 2> garb_stderr
# ./a.out eval_online > eval_stdout 2> eval_stderr

