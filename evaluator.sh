NTIMES=10
CTYPE=STANDARD #CTYPE=SCMC
mkdir -p logs

prog=./src/compgc

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$(readlink -f build)/lib

for TYPE in AES CBC WDBC CREDIT NURSERY_DT ECG_DT WDBC_NB NURSERY_NB
do
    rm files/evaluator_gcs/*; 
    sleep 1.0
    $prog --type $TYPE --eval-off 
    echo -e "\n$TYPE Online\n"
    sleep 2.0
    $prog --type $TYPE --eval-on
done

# valgrind --leak-check=full ./src/compgc --eval-off --type $TYPE
