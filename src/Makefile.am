AUTOMAKE_OPTIONS = foreign -Wall

SOURCES = \
2pc_aes.c \
2pc_and.c \
2pc_cbc.c \
2pc_evaluator.c \
2pc_function_spec.c \
2pc_garbled_circuit.c \
2pc_garbler.c \
2pc_hyperplane.c \
2pc_leven.c \
2pc_old_tests.c \
2pc_tests.c \
components.c \
crypto.c \
gc_comm.c \
gmputils.c \
ml_models.c \
net.c \
ot_np.c \
state.c \
utils.c

AM_CFLAGS = $(EXTRA_CFLAGS) -msse4.1 -maes -march=native
AM_LDFLAGS = -lgarble -lgarblec

bin_PROGRAMS = compgc
compgc_SOURCES = main.c $(SOURCES)

