#include <malloc.h>
#include <sys/socket.h>
#include <assert.h>

#include "gc_comm.h"
#include "net.h"

int
gc_comm_send(int sock, garble_circuit *gc)
{
    const size_t size = garble_size(gc, true, false);
    char *buf = garble_to_buffer(gc, NULL, true, false);
    net_send(sock, &size, sizeof size, 0);
    net_send(sock, buf, size, 0);
    free(buf);
    return 0;
}

int
gc_comm_recv(int sock, garble_circuit *gc)
{
    size_t size;
    char *buf;

    net_recv(sock, &size, sizeof size, 0);
    buf = malloc(size);
    net_recv(sock, buf, size, 0);
    garble_from_buffer(gc, buf, true, false);

    free(buf);
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
