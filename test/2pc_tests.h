#ifndef TWOPC_TESTS_H
#define TWOPC_TESTS_H

static int levenshteinDistance(int *s1, int *s2, int l);
static int lessThanCheck(int *inputs, int nints);
static void minCheck(int *inputs, int nints, int *output);
static void notGateTest();
static void minTest();
static void MUXTest();
static void LESTest(int n);
static void levenTest(int l, int sigma);
static void levenCoreTest();
static void saveAndLoadTest();

void runAllTests(void);

#endif
