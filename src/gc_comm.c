#include <malloc.h>
#include <sys/socket.h>

#include "gc_comm.h"

int
gc_comm_send(int sock, GarbledCircuit *gc)
{
    send(sock, &gc->n, sizeof gc->n, 0);
    send(sock, &gc->m, sizeof gc->m, 0);
    send(sock, &gc->q, sizeof gc->q, 0);
    send(sock, &gc->r, sizeof gc->r, 0);
    send(sock, gc->garbledGates, sizeof(GarbledGate) * gc->q, 0);
    send(sock, gc->garbledTable, sizeof(GarbledTable) * gc->q, 0);
    send(sock, gc->wires, sizeof(Wire) * gc->r, 0);
    send(sock, gc->outputs, sizeof(int) * gc->m, 0);

    return 0;
}

int
gc_comm_recv(int sock, GarbledCircuit *gc)
{
    (void) recv(sock, &gc->n, sizeof gc->n, 0);
    (void) recv(sock, &gc->m, sizeof gc->m, 0);
    (void) recv(sock, &gc->q, sizeof gc->q, 0);
    (void) recv(sock, &gc->r, sizeof gc->r, 0);
    gc->garbledGates =
        (GarbledGate *) memalign(128, sizeof(GarbledGate) * gc->q);
    gc->garbledTable =
        (GarbledTable *) memalign(128, sizeof(GarbledTable) * gc->q);
    gc->wires = (Wire *) memalign(128, sizeof(Wire) * gc->r);
    gc->outputs = (int *) memalign(128, sizeof(int) * gc->m);
    (void) recv(sock, gc->garbledGates, sizeof(GarbledGate) * gc->q, 0);
    (void) recv(sock, gc->garbledTable, sizeof(GarbledTable) * gc->q, 0);
    (void) recv(sock, gc->wires, sizeof(Wire) * gc->r, 0);
    (void) recv(sock, gc->outputs, sizeof(int) * gc->m, 0);

    return 0;
}
