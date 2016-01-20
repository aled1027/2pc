
# -w to ignore warnings
# getting warnings about redefining block -- 
# previous definitions of typedef __m128i block;
# turned off warning with -Wno-typedef-redefinition, for now.

SRCDIR := src
OBJDIR := obj
TESTDIR := test
BINDIR := bin

rm = rm --f

JUSTGARBLE = JustGarble
SOURCES := $(wildcard $(SRCDIR)/*.c)
OBJECTS  := $(SOURCES:$(SRCDIR)/%.c=$(OBJDIR)/%.o)

TESTSOURCES := $(wildcard $(TESTDIR)/*.c)
TESTOBJECTS := $(TESTSOURCES:$(TESTDIR)/%.c=$(OBJDIR)/%.o)

JUSTGARBLESRC := $(wildcard $(JUSTGARBLE)/src/*.c)
CIRCUITSRC := $(wildcard $(JUSTGARBLE)/circuit/*.c)

INCLUDES := $(wildcard $(SRCDIR)/*.h)
IDIR =include

CC=gcc
CFLAGS= -g -Wall -Iinc -I$(JUSTGARBLE)/include -I$(IDIR) -Wno-typedef-redefinition -Wno-unused-function -maes -msse4 -march=native -std=gnu11
LIBS=-lmsgpack -lm -lcrypto -lssl -lgmp -ljansson

AES = 2pc_aes
CBC = 2pc_cbc
LEVEN = 2pc_leven
MISC_TESTS = 2pc_misc_tests


###############
# COMPILATION #
###############
all: test

test: $(OBJECTS) $(TESTOBJECTS) $(TESTDIR)/2pc_misc_tests.c
	$(CC) $(SOURCES) $(JUSTGARBLESRC) $(CIRCUITSRC) $(TESTSOURCES) -o $(BINDIR)/test $(LIBS) $(CFLAGS) 

LEVEN: $(OBJECTS) $(TESTDIR)/$(LEVEN).c
	$(CC) $(JUSTGARBLESRC) $(OBJFULL) $(TESTDIR)/$(LEVEN).c -o $(BINDIR)/$(LEVEN) $(LIBS) $(CFLAGS) 

MISC_TESTS: $(OBJECTS) $(TESTDIR)/$(MISC_TESTS).c
	$(CC) $(JUSTGARBLESRC) $(OBJFULL) $(TESTDIR)/$(MISC_TESTS).c -o $(BINDIR)/$(MISC_TESTS) $(LIBS) $(CFLAGS) 

$(OBJECTS): $(OBJDIR)/%.o : $(SRCDIR)/%.c
	$(CC) -c $< -o $@ $(CFLAGS) 
$(TESTOBJECTS): $(OBJDIR)/%.o : $(TESTDIR)/%.c
	$(CC) -c $< -o $@ $(CFLAGS) 

#################
# COMPONENT CBC #
#################
cbc_garb_off:
	gdb --args ./$(BINDIR)/test --garb-off --type CBC

cbc_eval_off:
	gdb --args ./$(BINDIR)/test --eval-off --type CBC

cbc_garb_on:
	gdb --args ./$(BINDIR)/test --garb-on --type CBC

cbc_eval_on:
	gdb --args ./$(BINDIR)/test --eval-on --type CBC

#################
# COMPONENT AES #
#################
aes_garb_off:
	gdb --args ./$(BINDIR)/test --garb-off --type AES

aes_eval_off:
	gdb --args ./$(BINDIR)/test --eval-off --type AES

aes_garb_on:
	gdb --args ./$(BINDIR)/test --garb-on --type AES

aes_eval_on:
	gdb --args ./$(BINDIR)/test --eval-on --type AES

#########
# LEVEN #
#########
leven_garb_off:
	gdb --args $(BINDIR)/test --garb-off --type LEVEN

leven_eval_off:
	gdb --args $(BINDIR)/test --eval-off --type LEVEN

leven_garb_on:
	gdb --args $(BINDIR)/test --garb-on --type LEVEN

leven_eval_on:
	gdb --args $(BINDIR)/test --eval-on --type LEVEN

##############
# MISC TESTS #
##############
valg:
	valgrind --leak-check=full ./$(BINDIR)/$(MISC_TESTS)

clean_gcs:
	rm -r files/garbler_gcs
	rm -r files/evaluator_gcs
	mkdir files/garbler_gcs
	mkdir files/evaluator_gcs

all_test:
	./$(BINDIR)/$(MISC_TESTS)


##########
# EXTRAS #
##########
.PHONEY: clean
clean:
	@$(rm) $(OBJDIR)/*.o
	@$(rm) $(BINDIR)/test

