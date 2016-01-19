#ifndef TWOPC_LEVEN_H
#define TWOPC_LEVEN_H

int levenNumEvalInputs(void);
int levenNumGarbInputs(void);
int levenNumOutputs(void);
int levenNumCircs(void);

void leven_garb_off();
void leven_garb_on();
void full_leven_garb();
void full_leven_eval();

#endif
