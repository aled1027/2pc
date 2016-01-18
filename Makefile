
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
LIBS=-lmsgpack -lm -lcrypto -lssl -lgmp -ljansson

AES = 2pc_aes
CBC = 2pc_cbc


###############
# COMPILATION #
###############
all: test

test: $(OBJECTS) $(TESTOBJECTS) $(TESTDIR)/main.c
	$(CC) $(SOURCES) $(JUSTGARBLESRC) $(CIRCUITSRC) $(TESTSOURCES) -o $(BINDIR)/test $(LIBS) $(CFLAGS) 

# AES: $(OBJECTS) $(TESTDIR)/$(AES).c
# 	$(CC) $(JUSTGARBLESRC) $(CIRCUITSRC) $(TESTDIR)/$(AES).c -o $(BINDIR)/$(AES) $(LIBS) $(CFLAGS) 

# CBC: $(OBJECTS) $(TESTDIR)/$(CBC).c
# 	$(CC) $(JUSTGARBLESRC) $(CIRCUITSRC) $(TESTDIR)/$(CBC).c -o $(BINDIR)/$(CBC) $(LIBS) $(CFLAGS) 

$(OBJECTS): $(OBJDIR)/%.o : $(SRCDIR)/%.c
	$(CC) -c $< -o $@ $(CFLAGS) 
$(TESTOBJECTS): $(OBJDIR)/%.o : $(TESTDIR)/%.c
	$(CC) -c $< -o $@ $(CFLAGS) 

#################
# COMPONENT CBC #
#################

cbc_garb_off:
	./$(BINDIR)/$(CBC) garb_offline

cbc_eval_off:
	./$(BINDIR)/$(CBC) eval_offline

cbc_garb_on:
	./$(BINDIR)/$(CBC) garb_online

cbc_eval_on:
	./$(BINDIR)/$(CBC) eval_online

cbc_gdb_garb:
	gdb --args $(BINDIR)/$(CBC) garb_online

cbc_gdb_eval:
	gdb --args $(BINDIR)/$(CBC) eval_online

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
aes_garb_off:
	./$(BINDIR)/$(AES) garb_offline

aes_eval_off:
	./$(BINDIR)/$(AES) eval_offline

aes_garb_on:
	./$(BINDIR)/$(AES) garb_online

aes_eval_on:
	./$(BINDIR)/$(AES) eval_online

aes_gdb_garb:
	gdb --args $(BINDIR)/$(AES) garb_online

aes_gdb_eval:
	gdb --args $(BINDIR)/$(AES) eval_online

aes_full_garb:
	gdb --args $(BINDIR)/$(AES) full_garb

aes_full_eval:
	gdb --args $(BINDIR)/$(AES) full_eval

##########
# EXTRAS #
##########
.PHONEY: clean
clean:
	@$(rm) $(OBJDIR)/*.o
	@$(rm) $(BINDIR)/test

