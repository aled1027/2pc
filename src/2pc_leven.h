#ifndef TWOPC_LEVEN_H
#define TWOPC_LEVEN_H

int levenNumEvalInputs(int l, int sigma);
int levenNumGarbInputs(int l, int sigma);
int levenNumOutputs(int l);
int levenNumCircs(int l);

void leven_garb_off(int l, int sigma, ChainingType chainingType);
void leven_garb_on(int l, int sigma, ChainingType chainingType, char *functionPath);

void leven_garb_full(int l, int sigma);
void leven_eval_full(int l, int sigma);
ChainedGarbledCircuit* leven_circuits(int l, int sigma);

#endif
