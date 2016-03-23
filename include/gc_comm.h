#ifndef __GC_COMM_H
#define __GC_COMM_H

#include <garble.h>
#include "2pc_garbled_circuit.h"

int gc_comm_send(int sock, garble_circuit *gc);
int gc_comm_recv(int sock, garble_circuit *gc);
int chained_gc_comm_send(int sock, ChainedGarbledCircuit *chained_gc, ChainingType chainingType);
int chained_gc_comm_recv(int sock, ChainedGarbledCircuit *chained_gc, ChainingType chainingType) ;

#endif
