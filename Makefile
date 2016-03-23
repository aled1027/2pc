SRCDIR := src
OBJDIR := obj
TESTDIR := test
BINDIR := bin

rm = rm --f

# JUSTGARBLE = JustGarble

SOURCES := $(wildcard $(SRCDIR)/*.c)
OBJECTS  := $(SOURCES:$(SRCDIR)/%.c=$(OBJDIR)/%.o)
TESTSOURCES := $(wildcard $(TESTDIR)/*.c)
TESTOBJECTS := $(TESTSOURCES:$(TESTDIR)/%.c=$(OBJDIR)/%.o)
# JGSRC := $(wildcard $(JUSTGARBLE)/src/*.c)
# JGOBJECTS  := $(JGSRC:$(JUSTGARBLE)/src/%.c=$(JUSTGARBLE)/src/%.o)
# CIRCUITSRC := $(wildcard $(JUSTGARBLE)/circuit/*.c)
# CIRCUITOBJECTS  := $(CIRCUITSRC:$(JUSTGARBLE)/circuit/%.c=$(JUSTGARBLE)/circuit/%.o)

IDIR =include
INCLUDES := $(wildcard $(SRCDIR)/*.h) -Iinc -I$(JUSTGARBLE)/include -I$(IDIR)

CC=gcc
CFLAGS= -g -O0 -Wall -maes -msse4 -march=native -std=gnu11 $(INCLUDES)
# TODO add -Wextra -pedantic and fix errors/warnings
# TODO get rid of -Wno-unused-result and other flags if no-error/warning flags possible
# Wno-format is for printing uint64_t as llu.
CFLAGS += -Wno-typedef-redefinition -Wno-unused-function -Wno-unused-result -Wno-strict-aliasing -Wno-format
#CFLAGS += -DNDDEBUG # removes all "assert()" at compile time

LIBS=-lmsgpackc -lm -lcrypto -lssl -lgmp -ljansson -lgarble -lgarblec
#LIB+= -DNDDEBUG # removes all "assert()" at compile time

###############
# COMPILATION #
###############
all: test

test: $(TESTOBJECTS) $(OBJECTS)
	$(CC) $(OBJECTS) $(TESTOBJECTS) -o $(BINDIR)/test $(CFLAGS) $(LIBS)

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
	gdb --args ./$(BINDIR)/test --garb-on --type AES

aes_eval_on:
	gdb --args ./$(BINDIR)/test --eval-on --type AES 

#########
# LEVEN #
#########
leven_garb_off:
	$(BINDIR)/test --garb-off --type LEVEN

leven_eval_off:
	$(BINDIR)/test --eval-off --type LEVEN

leven_garb_on:
	gdb --args $(BINDIR)/test --garb-on --type LEVEN

leven_eval_on:
	gdb --args $(BINDIR)/test --eval-on --type LEVEN

##########
# EXTRAS #
##########
valg:
	valgrind --leak-check=full  ./$(BINDIR)/test --garb-on --type AES

clean_gcs:
	rm -r files/garbler_gcs
	rm -r files/evaluator_gcs
	mkdir files/garbler_gcs
	mkdir files/evaluator_gcs

run_tests:
	gdb --args $(BINDIR)/test --test


.PHONEY: clean
clean:
	@$(rm) $(OBJDIR)/*.o
	@$(rm) $(BINDIR)/test

