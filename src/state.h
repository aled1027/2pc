#ifndef __OTLIB_STATE_H__
#define __OTLIB_STATE_H__

#include <gmp.h>

#include "gmputils.h"

struct state {
    struct params p;
};

extern const unsigned int field_size;

int
state_init(struct state *s);

void
state_cleanup(struct state *s);

#endif
