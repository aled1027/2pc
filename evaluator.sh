#!/bin/bash

NTIMES=100
CTYPE=STANDARD
#TYPE=AES
TYPE=WDBC

./bin/test --chaining $CTYPE --type $TYPE --times $NTIMES --eval-full  2> logs/$TYPE-eval-full.txt

#./bin/test --chaining $CTYPE --type $TYPE --times $NTIMES --eval-off 2> logs/$TYPE-eval-off.txt
#sleep 1.5
#./bin/test --chaining $CTYPE --type $TYPE --times $NTIMES --eval-on 2> logs/$TYPE-eval-on.txt
#./bin/test --chaining $CTYPE --type $TYPE --times $NTIMES --eval-on





