NTIMES=10
CTYPE=STANDARD #CTYPE=SCMC
mkdir -p logs

prog=./src/compgc

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$(readlink -f build)/lib

for TYPE in AES CBC WDBC CREDIT NURSERY_DT ECG_DT WDBC_NB NURSERY_NB
do
    rm files/garbler_gc/*;
    $prog --type $TYPE --garb-off 
    echo -e "\n$TYPE Online\n"
    sleep 0.5
    $prog --type $TYPE --garb-on
done

#valgrind --leak-check=full ./src/compgc --garb-off --type AES

