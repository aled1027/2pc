
from pprint import pprint

def dist():
    a = "01 00 11".split()
    b = "00 01 00".split()
    l = 3

    D = [[0 for _ in range(l+1)] for _ in range(l+1)]

    for i in range(l+1):
        D[i][0] = i

    for j in range(l+1):
        D[0][j] = j

    for i in range(1,l+1):
        for j in range(1,l+1):
            t = 0 if a[i-1] == b[j-1] else 1
            print("comparing",i,j,t)
            D[i][j] = min(D[i-1][j] + 1, D[i][j-1] + 1, D[i-1][j-1] + t)

    pprint(D)
    print(D[-1][-1])
