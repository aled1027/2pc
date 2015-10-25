#include <malloc.h>
#include <sys/socket.h>

#include "gc_comm.h"
#include "net.h"

int
gc_comm_send(int sock, GarbledCircuit *gc)
{
    net_send(sock, &gc->n, sizeof gc->n, 0);
    net_send(sock, &gc->m, sizeof gc->m, 0);
    net_send(sock, &gc->q, sizeof gc->q, 0);
    net_send(sock, &gc->r, sizeof gc->r, 0);
    net_send(sock, gc->garbledGates, sizeof(GarbledGate) * gc->q, 0);
    net_send(sock, gc->garbledTable, sizeof(GarbledTable) * gc->q, 0);
    net_send(sock, gc->wires, sizeof(Wire) * gc->r, 0);
    net_send(sock, gc->outputs, sizeof(int) * gc->m, 0);
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
    gc->garbledGates =
        (GarbledGate *) memalign(128, sizeof(GarbledGate) * gc->q);
    gc->garbledTable =
        (GarbledTable *) memalign(128, sizeof(GarbledTable) * gc->q);
    gc->wires = (Wire *) memalign(128, sizeof(Wire) * gc->r);
    gc->outputs = (int *) memalign(128, sizeof(int) * gc->m);
    net_recv(sock, gc->garbledGates, sizeof(GarbledGate) * gc->q, 0);
    net_recv(sock, gc->garbledTable, sizeof(GarbledTable) * gc->q, 0);
    net_recv(sock, gc->wires, sizeof(Wire) * gc->r, 0);
    net_recv(sock, gc->outputs, sizeof(int) * gc->m, 0);
    net_recv(sock, &gc->globalKey, sizeof(block), 0);

    return 0;
}

int
chained_gc_comm_send(int sock, ChainedGarbledCircuit *chained_gc)
{
    gc_comm_send(sock, &chained_gc->gc);
    net_send(sock, &chained_gc->id, sizeof(chained_gc->id), 0);
    net_send(sock, &chained_gc->type, sizeof(chained_gc->type), 0);
    return 0;
}

int 
chained_gc_comm_recv(int sock, ChainedGarbledCircuit *chained_gc) 
{
    gc_comm_recv(sock, &chained_gc->gc);
    net_recv(sock, &chained_gc->id, sizeof(chained_gc->id), 0);
    net_recv(sock, &chained_gc->type, sizeof(chained_gc->type), 0);
    return 0;
}
