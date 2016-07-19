#!/bin/bash

NTIMES=1
CTYPE=STANDARD
#TYPE=NURSERY_DT
TYPE=ECG_DT

#gdb --args ./bin/test --chaining $CTYPE --type $TYPE --times $NTIMES --garb-full
#./bin/test --chaining $CTYPE --type $TYPE --times $NTIMES --garb-full

#gdb --args ./bin/test --chaining $CTYPE --type $TYPE --times $NTIMES --garb-off
#gdb -- args ./bin/test --chaining $CTYPE --type $TYPE --times $NTIMES --garb-off 2> logs/$TYPE-garb-off.txt
#sleep 1.5
#./bin/test --chaining $CTYPE --type $TYPE --times $NTIMES --garb-on  2> logs/$TYPE-garb-on.txt
gdb --args ./bin/test --chaining $CTYPE --type $TYPE --times $NTIMES --garb-on 
