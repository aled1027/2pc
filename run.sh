NTIMES=10

mkdir -p logs

for TYPE in CBC
do

    echo -e "\n$TYPE Offline/Online\n"

    ./bin/test --type $TYPE --garb-off 2> logs/$TYPE-garb-off.txt 1>/dev/null &
    sleep 0.5
    ./bin/test --type $TYPE --eval-off 2> logs/$TYPE-eval-off.txt 1>/dev/null
    sleep 0.5
    ./bin/test --type $TYPE --times $NTIMES --garb-on  2> logs/$TYPE-garb-on.txt &
    sleep 0.5
    ./bin/test --type $TYPE --times $NTIMES --eval-on 2> logs/$TYPE-eval-on.txt

    echo -e "\n$TYPE Full\n"

    sleep 0.5
    ./bin/test --type $TYPE --times $NTIMES --garb-full  2> logs/$TYPE-garb-full.txt &
    sleep 0.5
    ./bin/test --type $TYPE --times $NTIMES --eval-full 2> logs/$TYPE-eval-full.txt
    sleep 1.0

done
