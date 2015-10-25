#ifndef __GC_COMM_H
#define __GC_COMM_H

#include "justGarble.h"
#include "2pc_garbled_circuit.h"

int gc_comm_send(int sock, GarbledCircuit *gc);
int gc_comm_recv(int sock, GarbledCircuit *gc);
int chained_gc_comm_send(int sock, ChainedGarbledCircuit *chained_gc);
int chained_gc_comm_recv(int sock, ChainedGarbledCircuit *chained_gc) ;

#endif
