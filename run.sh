NTIMES=10

mkdir -p logs

for TYPE in AES CBC
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

TYPE="LEVEN"
for NSYMBOLS in 30 60
do

    echo -e "\n$TYPE-$NSYMBOLS Offline/Online\n"

    ./bin/test --type $TYPE --nsymbols $NSYMBOLS --garb-off 2> logs/$TYPE-garb-off.txt 1>/dev/null &
    sleep 1.0
    ./bin/test --type $TYPE --nsymbols $NSYMBOLS --eval-off 2> logs/$TYPE-eval-off.txt 1>/dev/null
    sleep 1.0
    ./bin/test --type $TYPE --nsymbols $NSYMBOLS --times $NTIMES --garb-on  2> logs/$TYPE-garb-on.txt &
    sleep 1.0
    ./bin/test --type $TYPE --nsymbols $NSYMBOLS --times $NTIMES --eval-on 2> logs/$TYPE-eval-on.txt
    sleep 1.0

    echo -e "\n$TYPE-$NSYMBOLS Full\n"

    ./bin/test --type $TYPE --nsymbols $NSYMBOLS --times $NTIMES --garb-full  2> logs/$TYPE-garb-full.txt &
    sleep 1.0
    ./bin/test --type $TYPE --nsymbols $NSYMBOLS --times $NTIMES --eval-full 2> logs/$TYPE-eval-full.txt
    sleep 1.0

done
