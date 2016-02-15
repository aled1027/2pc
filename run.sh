NTIMES=10
TYPE=AES

mkdir -p logs

./bin/test --type $TYPE --garb-off 2> logs/$TYPE-garb-off.txt &
sleep 0.5
./bin/test --type $TYPE --eval-off 2> logs/$TYPE-eval-off.txt
sleep 0.5
./bin/test --type $TYPE --times $NTIMES --garb-on  2> logs/$TYPE-garb-on.txt &
sleep 0.5
./bin/test --type $TYPE --times $NTIMES --eval-on 2> logs/$TYPE-eval-on.txt
