# sudo tc qdisc add dev lo root handle 1: tbf rate 50mbit burst 100000 limit 10000
sudo tc qdisc add dev lo root handle 1: netem delay 20msec
