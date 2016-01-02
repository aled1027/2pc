
# -w to ignore warnings
# getting warnings about redefining block -- 
# previous definitions of typedef __m128i block;
# turned off warning with -Wno-typedef-redefinition, for now.

SRCDIR := src
OBJDIR := obj
OBJFULL := obj/*.o
TESTDIR := test
BINDIR := bin

rm = rm --f

JUSTGARBLE = JustGarble
SOURCES := $(wildcard $(SRCDIR)/*.c)
OBJECTS  := $(SOURCES:$(SRCDIR)/%.c=$(OBJDIR)/%.o)
INCLUDES := $(wildcard $(SRCDIR)/*.h)
IDIR =include
JUSTGARBLESRC := $(wildcard $(JUSTGARBLE)/src/*.c)

CC=clang
CFLAGS= -Iinc -I$(JUSTGARBLE)/include -I$(IDIR) -Wno-typedef-redefinition -Wno-unused-function -maes -msse4 -march=native -g
LIBS=-lmsgpack -lm -lcrypto -lssl -lgmp -ljansson

AES = 2pc_aes
CBC = 2pc_cbc

all: AES CBC

CBC: $(OBJECTS) $(TESTDIR)/$(CBC).c
	$(CC) $(JUSTGARBLESRC) $(OBJFULL) $(TESTDIR)/$(CBC).c -o $(BINDIR)/$(CBC) $(LIBS) $(CFLAGS) 


cbc_garb_off:
	./$(BINDIR)/$(CBC) garb_offline

cbc_eval_off:
	./$(BINDIR)/$(CBC) eval_offline

cbc_gdb_garb:
	gdb --args $(BINDIR)/$(CBC) garb_online

cbc_gdb_eval:
	gdb --args $(BINDIR)/$(CBC) eval_online

AES: $(OBJECTS) $(TESTDIR)/$(AES).c
	$(CC) $(JUSTGARBLESRC) $(OBJFULL) $(TESTDIR)/$(AES).c -o $(BINDIR)/$(AES) $(LIBS) $(CFLAGS) 

aes_garb_off:
	./$(BINDIR)/$(AES) garb_offline

aes_eval_off:
	./$(BINDIR)/$(AES) eval_offline

aes_gdb_garb:
	gdb --args $(BINDIR)/$(AES) garb_online

aes_gdb_eval:
	gdb --args $(BINDIR)/$(AES) eval_online

$(OBJECTS): $(OBJDIR)/%.o : $(SRCDIR)/%.c
	$(CC) -c $< -o $@ $(CFLAGS) 

.PHONEY: clean
clean:
	@$(rm) $(OBJECTS)
	@$(rm) $(BINDIR)/$(AES)

###### OLD ######
#
#all: 
#	$(OBJECTS) $(CC) $(JUSTGARBLESRC) $(SOURCES) $(CFLAGS) $(LIBS) 
#
#gdb_garb_off:
#	gdb --args a.out garb_offline
#
#run_garb_off:
#	./a.out garb_offline
#
#run_eval_off:
#	./a.out eval_offline
#
#run_eval:
#	./a.out eval_online
#
#run_garb:
#	./a.out garb_online
#
#run_tests: 
#	./a.out tests
#
#gdb_tests:
#	gdb --args a.out tests
#
#valgrind: 
#	make;
#	rm valg.out;
#	valgrind -v --leak-check=full --show-leak-kinds=all --track-origins=yes --leak-check=yes --log-file=valg.out ./a.out garb_online;
#
#gdb_garb:
#	gdb --args a.out garb_online
#
#gdb_eval:
#	gdb --args a.out eval_online
#
## -q
## -v
#
#.PHONEY: clean
#clean:
#	#rm garbler evaluator a.out; 
#	rm -rf files/evaluator_gcs; 
#	rm -rf files/garbler_gcs;
#	mkdir files/evaluator_gcs;
#	mkdir files/garbler_gcs;

# ./a.out garb_online > garb_stdout 2> garb_stderr
# ./a.out eval_online > eval_stdout 2> eval_stderr

