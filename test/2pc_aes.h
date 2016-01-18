#ifndef TWOPC_AES
#define TWOPC_AES

#include <stdbool.h>

void aes_garb_off();
void aes_garb_on(char* function_path, bool timing);
void aes_eval_off();
void aes_eval_on(bool timing);
void full_aes_garb();
void full_aes_eval();

#endif
