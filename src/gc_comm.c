#include <malloc.h>
#include <sys/socket.h>
#include <assert.h>

#include "gc_comm.h"
#include "net.h"

int
gc_comm_send(int sock, GarbledCircuit *gc)
{
    net_send(sock, &gc->n, sizeof gc->n, 0);
    net_send(sock, &gc->m, sizeof gc->m, 0);
    net_send(sock, &gc->q, sizeof gc->q, 0);
    net_send(sock, &gc->r, sizeof gc->r, 0);
    net_send(sock, &gc->nFixedWires, sizeof gc->nFixedWires, 0);

    net_send(sock, gc->gates, sizeof(Gate) * gc->q, 0);
    net_send(sock, gc->garbledTable, sizeof(GarbledTable) * gc->q, 0);
    net_send(sock, gc->outputs, sizeof(int) * gc->m, 0);
    net_send(sock, gc->fixedWires, sizeof(int) * gc->nFixedWires, 0);
    net_send(sock, &gc->fixedLabel, sizeof(block), 0);
    net_send(sock, &gc->globalKey, sizeof(block), 0);

    return 0;
}

int
gc_comm_recv(int sock, GarbledCircuit *gc)
{
    net_recv(sock, &gc->n, sizeof gc->n, 0);
    net_recv(sock, &gc->m, sizeof gc->m, 0);
    net_recv(sock, &gc->q, sizeof gc->q, 0);
    net_recv(sock, &gc->r, sizeof gc->r, 0);
    net_recv(sock, &gc->nFixedWires, sizeof gc->nFixedWires, 0);

    gc->gates = calloc(gc->q, sizeof(Gate));
    gc->garbledTable = calloc(gc->q, sizeof(GarbledTable));
    gc->outputs = calloc(gc->m, sizeof(int));
    gc->fixedWires = calloc(gc->nFixedWires, sizeof(int));

    net_recv(sock, gc->gates, sizeof(Gate) * gc->q, 0);
    net_recv(sock, gc->garbledTable, sizeof(GarbledTable) * gc->q, 0);
    net_recv(sock, gc->outputs, sizeof(int) * gc->m, 0);
    net_recv(sock, gc->fixedWires, sizeof(int) * gc->nFixedWires, 0);
    net_recv(sock, &gc->fixedLabel, sizeof(block), 0);
    net_recv(sock, &gc->globalKey, sizeof(block), 0);

    gc->wires = NULL;

    return 0;
}

int
chained_gc_comm_send(int sock, ChainedGarbledCircuit *chained_gc, ChainingType chainingType)
{
    FILE* fp = stdout;
    gc_comm_send(sock, &chained_gc->gc);
    net_send(sock, &chained_gc->id, sizeof(chained_gc->id), 0);
    net_send(sock, &chained_gc->type, sizeof(chained_gc->type), 0);

    printf("in send\n");
    if (chainingType == CHAINING_TYPE_SIMD) {
        assert(chained_gc->offlineChainingOffsets && "offlineChainingOffsets should be allocated");
        net_send(sock, chained_gc->offlineChainingOffsets, sizeof(block) * chained_gc->gc.m, 0);

        print_block(fp, chained_gc->offlineChainingOffsets[0]);
    }
    return 0;
}

int 
chained_gc_comm_recv(int sock, ChainedGarbledCircuit *chained_gc, ChainingType chainingType) 
{
    gc_comm_recv(sock, &chained_gc->gc);
    chained_gc->gc.wires = NULL;
    net_recv(sock, &chained_gc->id, sizeof(chained_gc->id), 0);
    net_recv(sock, &chained_gc->type, sizeof(chained_gc->type), 0);

    printf("in recv\n");
    if (chainingType == CHAINING_TYPE_SIMD) {
        chained_gc->offlineChainingOffsets = allocate_blocks(chained_gc->gc.m);
        net_recv(sock, chained_gc->offlineChainingOffsets, sizeof(block) * chained_gc->gc.m, 0);
    }
    
    return 0;
}
