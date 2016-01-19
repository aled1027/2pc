#ifndef TWOPC_CBC_H
#define TWOPC_CBC_H

extern int NUM_AES_ROUNDS;
extern int NUM_CBC_BLOCKS;

int cbcNumGarbInputs(void);
int cbcNumEvalInputs(void);
int cbcNumCircs(void);
int cbcNumOutputs(void);

void cbc_garb_off(char *dir);

#endif
