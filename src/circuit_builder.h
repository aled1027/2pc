#ifndef circuit_builder_h
#define circuit_builder_h
#include <assert.h>
#include <malloc.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <msgpack.h>
#include <sys/stat.h>


#include "gc_comm.h"
#include "gates.h"
#include "circuits.h"
#include "utils.h"

#define FAILURE -1
#define SUCCESS 0

struct Alex {
    int n,m,q,r;
    //block* obj;
};

int mySave(struct Alex *gc, char* fileName) ;
int myLoad(struct Alex *gc, char* fileName) ;
void unpack(char const* buf, size_t len);
void buildAdderCircuit(GarbledCircuit *gc);
int saveBlock(block* bl, char* fileName);

#endif
