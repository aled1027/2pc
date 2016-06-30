#!/bin/bash

NTIMES=100
CTYPE=STANDARD
#TYPE=AES
TYPE=WDBC

./bin/test --chaining $CTYPE --type $TYPE --times $NTIMES --garb-full  2> logs/$TYPE-garb-full.txt 

#./bin/test --chaining $CTYPE --type $TYPE --times $NTIMES --garb-off 2> logs/$TYPE-garb-off.txt
#sleep 1.5
#./bin/test --chaining $CTYPE --type $TYPE --times $NTIMES --garb-on  2> logs/$TYPE-garb-on.txt
#./bin/test --chaining $CTYPE --type $TYPE --times $NTIMES --garb-on 
