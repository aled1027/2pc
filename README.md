# CompGC
offline/online 2PC

## Setup

There may be unlisted dependencies.

```
sudo apt-get install libjannson4 libmsgpack-dev libssl-dev
./build.sh
```

## Example Usage

In one terminal:
```
./src/compgc --garb-full --type AES
./src/compgc --garb-off --type AES
./src/compgc --garb-on --type AES
```

In a different terminal:
```
./src/compgc --eval-full --type AES
./src/compgc --eval-off --type AES
./src/compgc --eval-on --type AES
```

## Miscellaneous Details
- Values encoded with signed little-endian where least signficant bit is on the left. 
- wdbc
    - inputs multiplied by `10**17`
    - used `num_len = 55`
- credit
    - inputs multiplied by `10**18`
    - used `num_len = 58`
    
