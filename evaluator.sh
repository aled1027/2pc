#!/bin/bash

NTIMES=1
CTYPE=STANDARD
#TYPE=NURSERY_DT
TYPE=ECG_DT

#gdb --args ./bin/test --chaining $CTYPE --type $TYPE --times $NTIMES --eval-full
#./bin/test --chaining $CTYPE --type $TYPE --times $NTIMES --eval-full

#gdb --args ./bin/test --chaining $CTYPE --type $TYPE --times $NTIMES --eval-off
#gdb --args ./bin/test --chaining $CTYPE --type $TYPE --times $NTIMES --eval-off 2> logs/$TYPE-eval-off.txt
#sleep 1.5
#./bin/test --chaining $CTYPE --type $TYPE --times $NTIMES --eval-on 2> logs/$TYPE-eval-on.txt
gdb --args ./bin/test --chaining $CTYPE --type $TYPE --times $NTIMES --eval-on





