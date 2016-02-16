#ifndef TWOPC_LEVEN_H
#define TWOPC_LEVEN_H

int levenNumEvalInputs(void);
int levenNumGarbInputs(void);
int levenNumOutputs(void);
int levenNumCircs(void);
int levenNumEvalLabels(void);

void leven_garb_off(ChainingType chainingType);
void leven_garb_on(ChainingType chainingType);

void leven_garb_full(void);
void leven_eval_full(void);

#endif
