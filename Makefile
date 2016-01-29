
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
CFLAGS= -O2 -Wall -Iinc -I$(JUSTGARBLE)/include -I$(IDIR) -maes -msse4 -march=native -std=gnu11
# TODO get rid of -Wno-unused-result and other flags if no-error/warning flags possible
CFLAGS += -Wno-typedef-redefinition -Wno-unused-function -Wno-unused-result -Wno-strict-aliasing

LIBS=-lmsgpack -lm -lcrypto -lssl -lgmp -ljansson 
#LIBS=-lmsgpackc -lm -lcrypto -lssl -lgmp -ljansson # for Alex L (libmsgpackc)
#LIB+= -DNDDEBUG # removes all "assert()" at compile time

AES = 2pc_aes
CBC = 2pc_cbc
LEVEN = 2pc_leven
MISC_TESTS = 2pc_tests


###############
# COMPILATION #
###############
all: test

test: $(OBJECTS) $(TESTOBJECTS) $(TESTDIR)/2pc_tests.c
	$(CC) $(SOURCES) $(JUSTGARBLESRC) $(CIRCUITSRC) $(TESTSOURCES) -o $(BINDIR)/test $(LIBS) $(CFLAGS)

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
	./$(BINDIR)/test --garb-off --type AES

aes_eval_off:
	./$(BINDIR)/test --eval-off --type AES

aes_garb_on:
	./$(BINDIR)/test --garb-on --type AES --times 1
aes_eval_on:
	./$(BINDIR)/test --eval-on --type AES --times 1
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

##########
# EXTRAS #
##########
valg:
	valgrind --leak-check=full ./$(BINDIR)/$(MISC_TESTS)

clean_gcs:
	rm -r files/garbler_gcs
	rm -r files/evaluator_gcs
	mkdir files/garbler_gcs
	mkdir files/evaluator_gcs

all_test:
	gdb --args $(BINDIR)/test --test


.PHONEY: clean
clean:
	@$(rm) $(OBJDIR)/*.o
	@$(rm) $(BINDIR)/test

