#ifndef TWOPC_AES
#define TWOPC_AES

#include <stdbool.h>

int aesNumGarbInputs(void);
int aesNumEvalInputs(void);
int aesNumCircs(void);
int aesNumOutputs(void);

void aes_garb_off(char *dir, int nchains);

#endif
