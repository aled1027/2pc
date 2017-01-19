NTIMES=10
CTYPE=STANDARD #CTYPE=SCMC
mkdir -p logs

prog=./src/compgc

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$(readlink -f build)/lib

#for TYPE in AUD_NB WDBC CREDIT NURSERY_DT ECG_DT WDBC_NB NURSERY_NB AUD_NB
#do
#    sleep 0.5
#    $prog --chaining $CTYPE --type $TYPE --times $NTIMES --garb-full & # 2> logs/$TYPE-garb-full.txt &
#    sleep 0.5
#    $prog --chaining $CTYPE --type $TYPE --times $NTIMES --eval-full # 2> logs/$TYPE-eval-full.txt
#    sleep 1.0
#
#    echo -e "\n$TYPE Offline/Online\n"
#
#    rm files/garbler_gc/*;
#    rm files/evaluator_gcs/*; 
#
#    ./src/compgc --chaining $CTYPE --type $TYPE --garb-off & # 2> logs/$TYPE-garb-off.txt 1>/dev/null &
#    sleep 0.5
#    ./src/compgc --chaining $CTYPE --type $TYPE --eval-off # 2> logs/$TYPE-eval-off.txt 1>/dev/null
#    echo -e "\n$TYPE Online\n"
#    sleep 0.5
#    ./src/compgc --chaining $CTYPE --type $TYPE --times $NTIMES --garb-on & # 2> logs/$TYPE-garb-on.txt &
#    sleep 1.5
#    ./src/compgc --chaining $CTYPE --type $TYPE --times $NTIMES --eval-on # 2> logs/$TYPE-eval-on.txt
#done

#for TYPE in WDBC CREDIT NURSERY_DT ECG_DT WDBC_NB NURSERY_NB
for TYPE in AES
do
    rm files/garbler_gc/*;
    $prog --type $TYPE --garb-off 
    echo -e "\n$TYPE Online\n"
    sleep 0.5
    $prog --type $TYPE --garb-on
done

