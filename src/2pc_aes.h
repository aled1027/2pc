#ifndef TWOPC_AES
#define TWOPC_AES

#include <stdbool.h>
#include "garble.h"

int aesNumGarbInputs(void);
int aesNumEvalInputs(void);
int aesNumCircs(void);
int aesNumOutputs(void);

void aes_garb_off(char *dir, int nchains, ChainingType chainingType);
ChainedGarbledCircuit* aes_circuits(int nchains, ChainingType chainingType);

#endif
