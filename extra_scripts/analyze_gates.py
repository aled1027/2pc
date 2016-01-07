

num_xor = 0
num_and = 0
num_zero = 0
num_one = 0

fn = "aes_round_gates.txt"

with open(fn) as f:
    for line in f:
        if "xor" in line:
            num_xor += 1
        elif "and" in line:
            num_and += 1
        elif "fixed_zero" in line:
            num_zero += 1
        elif "fixed_one" in line:
            num_one += 1
        else:
            print(line[0:-1])

print("num_xor", num_xor)
print("num_and", num_and)
print("num_zero", num_zero)
print("num_one", num_one)
print("num_xor + num_and", num_xor + num_and)
print("total", num_xor + num_and + num_zero + num_one)
print("percent_xor", float(num_xor) / float(num_xor + num_and))
