
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
CFLAGS= -g -Wall -Iinc -I$(JUSTGARBLE)/include -I$(IDIR) -Wno-typedef-redefinition -Wno-unused-function -maes -msse4 -march=native
LIBS=-lmsgpackc -lm -lcrypto -lssl -lgmp -ljansson

AES = 2pc_aes
CBC = 2pc_cbc
LEVEN = 2pc_leven
MISC_TESTS = 2pc_misc_tests


###############
# COMPILATION #
###############
all: test

test: $(OBJECTS) $(TESTOBJECTS) $(TESTDIR)/main.c
	$(CC) $(SOURCES) $(JUSTGARBLESRC) $(CIRCUITSRC) $(TESTSOURCES) -o $(BINDIR)/test $(LIBS) $(CFLAGS) 

MISC_TESTS: $(OBJECTS) $(TESTDIR)/$(MISC_TESTS).c
	$(CC) $(JUSTGARBLESRC) $(OBJFULL) $(TESTDIR)/$(MISC_TESTS).c -o $(BINDIR)/$(MISC_TESTS) $(LIBS) $(CFLAGS) 

$(OBJECTS): $(OBJDIR)/%.o : $(SRCDIR)/%.c
	$(CC) -c $< -o $@ $(CFLAGS) 
$(TESTOBJECTS): $(OBJDIR)/%.o : $(TESTDIR)/%.c
	$(CC) -c $< -o $@ $(CFLAGS) 

#################
# COMPONENT CBC #
#################

cbc_garb_on:
	gdb --args ./$(BINDIR)/$(CBC) garb_online

cbc_eval_on:
	gdb --args ./$(BINDIR)/$(CBC) eval_online

cbc_garb_off:
	gdb --args $(BINDIR)/$(CBC) garb_offline

cbc_eval_off:
	gdb --args $(BINDIR)/$(CBC) eval_offline

############
# FULL CBC #
############
full_cbc_garb_off:
	gdb --args $(BINDIR)/$(CBC) full_garb_off

full_cbc_garb_on:
	gdb --args $(BINDIR)/$(CBC) full_garb_on

full_cbc_eval_off:
	gdb --args $(BINDIR)/$(CBC) full_eval_off

full_cbc_eval_on:
	gdb --args $(BINDIR)/$(CBC) full_eval_on

#################
# COMPONENT AES #
#################
aes_garb_on:
	gdb --args $(BINDIR)/$(AES) garb_online

aes_garb_off:
	gdb --args $(BINDIR)/$(AES) garb_offline

aes_eval_on:
	gdb --args $(BINDIR)/$(AES) eval_online

aes_eval_off:
	gdb --args $(BINDIR)/$(AES) eval_offline

aes_full_garb:
	gdb --args $(BINDIR)/$(AES) full_garb

aes_full_eval:
	gdb --args $(BINDIR)/$(AES) full_eval

#########
# LEVEN #
#########
leven_full_garb:
	gdb --args $(BINDIR)/$(LEVEN) full_garb

leven_full_eval:
	gdb --args $(BINDIR)/$(LEVEN) full_eval

##############
# MISC TESTS #
##############
valg:
	valgrind --leak-check=full ./$(BINDIR)/$(AES) garb_online

all_tests:
	gdb ./$(BINDIR)/$(MISC_TESTS)

##########
# EXTRAS #
##########
.PHONEY: clean
clean:
	@$(rm) $(OBJDIR)/*.o
	@$(rm) $(BINDIR)/test

