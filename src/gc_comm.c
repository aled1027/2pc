#include <malloc.h>
#include <sys/socket.h>
#include <assert.h>

#include "gc_comm.h"
#include "net.h"

int
gc_comm_send(int sock, garble_circuit *gc)
{
    net_send(sock, &gc->n, sizeof gc->n, 0);
    net_send(sock, &gc->m, sizeof gc->m, 0);
    net_send(sock, &gc->q, sizeof gc->q, 0);
    net_send(sock, &gc->r, sizeof gc->r, 0);
    net_send(sock, &gc->n_fixed_wires, sizeof gc->n_fixed_wires, 0);
    net_send(sock, &gc->type, sizeof gc->type, 0);

    net_send(sock, gc->gates, sizeof(garble_gate) * gc->q, 0);
    net_send(sock, gc->table, garble_table_size(gc) * gc->q, 0);
    net_send(sock, gc->outputs, sizeof(int) * gc->m, 0);
    net_send(sock, gc->fixed_wires, sizeof(garble_fixed_wire) * gc->n_fixed_wires, 0);
    net_send(sock, &gc->fixed_label, sizeof(block), 0);
    net_send(sock, &gc->global_key, sizeof(block), 0);

    return 0;
}

int
gc_comm_recv(int sock, garble_circuit *gc)
{
    net_recv(sock, &gc->n, sizeof gc->n, 0);
    net_recv(sock, &gc->m, sizeof gc->m, 0);
    net_recv(sock, &gc->q, sizeof gc->q, 0);
    net_recv(sock, &gc->r, sizeof gc->r, 0);
    net_recv(sock, &gc->n_fixed_wires, sizeof gc->n_fixed_wires, 0);
    net_recv(sock, &gc->type, sizeof gc->type, 0);

    gc->gates = calloc(gc->q, sizeof(garble_gate));
    gc->table = calloc(gc->q, garble_table_size(gc));
    gc->outputs = calloc(gc->m, sizeof(int));
    gc->fixed_wires = calloc(gc->n_fixed_wires, sizeof(garble_fixed_wire));

    net_recv(sock, gc->gates, sizeof(garble_gate) * gc->q, 0);
    net_recv(sock, gc->table, garble_table_size(gc) * gc->q, 0);
    net_recv(sock, gc->outputs, sizeof(int) * gc->m, 0);
    net_recv(sock, gc->fixed_wires, sizeof(garble_fixed_wire) * gc->n_fixed_wires, 0);
    net_recv(sock, &gc->fixed_label, sizeof(block), 0);
    net_recv(sock, &gc->global_key, sizeof(block), 0);

    gc->wires = NULL;

    return 0;
}

int
chained_gc_comm_send(int sock, ChainedGarbledCircuit *chained_gc, ChainingType chainingType)
{
    gc_comm_send(sock, &chained_gc->gc);
    net_send(sock, &chained_gc->id, sizeof(chained_gc->id), 0);
    net_send(sock, &chained_gc->type, sizeof(chained_gc->type), 0);

    if (chainingType == CHAINING_TYPE_SIMD) {
        assert(chained_gc->offlineChainingOffsets && "offlineChainingOffsets should be allocated");
        net_send(sock, chained_gc->offlineChainingOffsets, sizeof(block) * chained_gc->gc.m, 0);
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

    if (chainingType == CHAINING_TYPE_SIMD) {
        chained_gc->offlineChainingOffsets = garble_allocate_blocks(chained_gc->gc.m);
        net_recv(sock, chained_gc->offlineChainingOffsets, sizeof(block) * chained_gc->gc.m, 0);
    }
    
    return 0;
}
