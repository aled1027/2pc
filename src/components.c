#include "components.h"
#include "utils.h"

#include "garble.h"
#include "circuits.h"
//#include "gates.h"

#include <string.h>
#include <assert.h>
#include <math.h>

void my_not_gate(garble_circuit *gc, garble_context *ctxt, int in, int out) 
{
	int fixed_wire_one = wire_one(gc);
	gate_XOR(gc, ctxt, in, fixed_wire_one, out);
}

void circuit_signed_negate(garble_circuit *gc, garble_context *ctxt, uint32_t n, int *input, int *output) 
{
    // not all bits, and put 1 in first half of add_in
    int zero_wire = wire_zero(gc);
    int add_in[2 * n];
    
    for (uint32_t i = 0; i < n; ++i) {
        add_in[n + i] = builder_next_wire(ctxt);
        my_not_gate(gc, ctxt, input[i], add_in[n + i]);
        add_in[i] = zero_wire;
    }
    add_in[n-1] = wire_one(gc);

    int carry; // can ignore
    circuit_add(gc, ctxt, 2 * n, add_in, output, &carry);
}

void my_or_gate(garble_circuit *gc, garble_context *ctxt, int in0, int in1, int out) 
{
    // OR gate because the libgarble one isn't working?
    // XOR(AND(in0, in1), XOR(in0, in1)))
    int and_out = builder_next_wire(ctxt);
    int xor_out = builder_next_wire(ctxt);

    gate_XOR(gc, ctxt, in0, in1, xor_out);
    gate_AND(gc, ctxt, in0, in1, and_out);
    gate_XOR(gc, ctxt, xor_out, and_out, out);
}

void circuit_signed_less_than(garble_circuit *gc, garble_context *ctxt, int n, int *input1, int *input2, int *output) 
{
    // Inputs are signed little-endian
    // |input1| = |input2| = n / 2 = num_len
    assert(n % 2 == 0);
    int split = n / 2;
    int les_in[n - 2];
    memcpy(les_in, input1 + 1, (split-1) * sizeof(int));
    memcpy(&les_in[split-1], input2 + 1, (split-1) * sizeof(int));

    int les_out;
	new_circuit_les(gc, ctxt, n-2, les_in, &les_out);

    //       les_out       middle_bool                  right_bool
    // out = [sign1 AND [NOT AND(sign2, les_out)]] OR [AND(NOT sign2, les_out)]
    
    int sign1 = input1[0];
    int sign2 = input2[0];
    int and_out = builder_next_wire(ctxt);
    int middle_bool = builder_next_wire(ctxt);
    int right_bool = builder_next_wire(ctxt);
    int not_sign2 = builder_next_wire(ctxt);
    int temp_and = builder_next_wire(ctxt);
    int final_out = builder_next_wire(ctxt);

    // middle
    gate_AND(gc, ctxt, sign2, les_out, and_out);
    my_not_gate(gc, ctxt, and_out, middle_bool);
    
    // right
    my_not_gate(gc, ctxt, sign2, not_sign2);
    gate_AND(gc, ctxt, not_sign2, les_out, right_bool);

    // OR(les_out, middle_bool, right_bool)
    gate_AND(gc, ctxt, sign1, middle_bool, temp_and);
    my_or_gate(gc, ctxt, temp_and, right_bool, final_out);
    *output = final_out;
}

static void
bitwiseMUX(garble_circuit *gc, garble_context *gctxt, int the_switch, const int *inps, 
		int ninputs, int *outputs)
{
	assert(ninputs % 2 == 0 && "ninputs should be even because we are muxing");
	assert(outputs && "final should already be malloced");
	int split = ninputs / 2;


	for (int i = 0; i < split; i++) {
		//circuit_mux21(gc, gctxt, the_switch, inps[i], inps[i + split], &outputs[i]);
		new_circuit_mux21(gc, gctxt, the_switch, inps[i], inps[i + split], &outputs[i]);
	}
}

void circuit_gr0(garble_circuit *gc, garble_context *ctxt,
        uint32_t n, int *inputs, int *output)
{
    /* Circuit that returns 1 (true) if inputs is greater than 0, else returns 0 (false). 
     * int n is the size of the input numbers in bits. 
     */

    // just do a NOT gate on (n-1)th input (the msb), which is the sign bit.
	*output = builder_next_wire(ctxt);
    my_not_gate(gc, ctxt, inputs[n-1], *output);
}

void 
circuit_inner_product(garble_circuit *gc, garble_context *ctxt, 
        uint32_t n, int num_len, int *inputs, int *outputs)
{
    /* Performs inner product over the well integers (well, mod 2^num_len)
     *
     * Assumes little-endian with sign. E.g. 1100 base 2 = -1 base 10
     *
     * Inputs
     * n: total number of input bits
     * num_len: length of each number in bits
     *
     * Variables
     * num_numbers: the number of numbers in the input
     * vector_length: the number of numbers in each vector
     * split: index in inputs where second vector begins
     */

    uint32_t split = n / 2;
    uint32_t vector_length = split / num_len;
    int left[split];
    int right[split];
    int add_in[2*num_len];
    int add_wires[num_len];
    int mult_in[2*num_len];
    int mult_out[num_len];
    int carry;

    // left and right vectors
    // e.g. we do <left, right> where left and right are vectors
    memcpy(left, inputs, split * sizeof(int));
    memcpy(right, &inputs[split], split * sizeof(int));

    // mult first two numbers
    memcpy(mult_in, left, num_len * sizeof(int)); 
    memcpy(&mult_in[num_len], right, num_len * sizeof(int)); 
    circuit_signed_mult_n(gc, ctxt, 2 * num_len, mult_in, add_wires);

    for (int i = 1; i < vector_length; ++i) {
        memcpy(mult_in, &left[i * num_len], num_len * sizeof(int)); 
        memcpy(&mult_in[num_len], &right[i * num_len], num_len * sizeof(int)); 

        circuit_signed_mult_n(gc, ctxt, 2 * num_len, mult_in, add_in);
        memcpy(&add_in[num_len], add_wires, num_len * sizeof(int));
		circuit_add(gc, ctxt, 2 * num_len, add_in, add_wires, &carry);
    }
    memcpy(outputs, add_wires, num_len * sizeof(int));

}

void circuit_select(garble_circuit *gc, garble_context *ctxt, int num_len,
        int input_array_size, int index_size, int *inputs, int *outputs)
{
    /* Builds a select circuit. 
     * It takes as input an array of array_size numbers, each of num_len bits, 
     * and an index, a value of num_len bits. 
     *
     * It returns the index'th value of the array, i.e. arr[idx]
     *
     * n (num_inputs) = (num_len * array_size) + array_size
     *
     * Algorithm: 
     * Loop over array of numbers, muxing every pair( e.g. mux(arr[0], arr[1]), mux(arr[2], arr[3]), ..
     * Put results into an int array.
     * Loop over new array, muxing each pair as before.
     * Eventually, there will only be one value in the array, return this value. 
     *
     * Warning: index_size is always truncated to the size of the tree. 
     * There is no use of the extra bits. 
     *
     * All inputs, include the index, should be signed little-endian.
     */
    assert(inputs && outputs);

    // set array_size to smallest power of 2 greater than input_array_size
    int array_size = pow(2, ceil(log2(input_array_size)));
    assert(input_array_size <= array_size);

    // array of switches in order that they will be used
    // add one and subtract one to ignore the sign bit
    int switches[index_size - 1];
    memcpy(switches, inputs + 1 + (num_len * input_array_size), (num_len - 1) * sizeof(int));
    
    // redoing tree_vals and new_tree_vals
    // going to lose memory with current scheme
    int tree_vals[num_len * array_size];
    memcpy(tree_vals, inputs, num_len * input_array_size * sizeof(int));
    for (int i = num_len * input_array_size; i < num_len * array_size; ++i) {
        tree_vals[i] = -1;
    }

    // loop from bottom to top of tree
    // tree has height array_size
    int tree_depth = floor(log2(array_size));
    assert(index_size - 1 >= tree_depth && "otherwise not enough bits to mux");
    for (int tree_level = 0; tree_level < tree_depth; tree_level++) {
        // num_nodes in tree at level
        int num_nodes = pow(2, tree_depth - tree_level);
        assert(tree_level < index_size);
        int the_switch = switches[tree_level];

        // initialize new_tree_vals to all -1
        int new_tree_size = num_len * (num_nodes / 2);
        int new_tree_vals[new_tree_size]; // values for next level of tree
        for (int i = 0; i < new_tree_size; i++) {
            new_tree_vals[i] = -1;
        }

        // loop over nodes two at a time
        assert(num_nodes % 2 == 0);
        for (int node = 0; node < num_nodes; node+=2) {
            if (tree_vals[num_len * node] == -1 && tree_vals[num_len * (node + 1)]) {
                // both values are negative one, so leave new_tree_vals as -1
                // do nothing
            } else if (tree_vals[num_len * (node + 1)] == -1) {
                // only one value is -1, so set new_tree_vals equal to 
                // the other value
                memcpy(new_tree_vals + (num_len * (node / 2)), tree_vals + (num_len * node), num_len * sizeof(int));
            } else {
                // both values are eligible, so use a mux to determine which value moves on.
                int mux_in[2 * num_len];
                memcpy(mux_in, &tree_vals[num_len * node], num_len * sizeof(int));
                memcpy(mux_in + num_len, &tree_vals[num_len * (node + 1)], num_len * sizeof(int));
                bitwiseMUX(gc, ctxt, the_switch, mux_in, num_len * 2, new_tree_vals + (num_len * (node / 2)));
            }
        }
        memcpy(tree_vals, new_tree_vals, new_tree_size * sizeof(int));
    }
    memcpy(outputs, tree_vals, num_len * sizeof(int));
}

void circuit_bitwiseMUX41(garble_circuit *gc, garble_context *ctxt,
        int num_len, int *inputs, int *outputs)
{
    /* A 4 to 1 for mux for inputs of size of num_len. 
     * 
     * On input of four numbers, each consisting of num_len bits, and two select bits,
     * this outputs the number indicated by the select bits.
     *
     * Formula MUX(s1, MUX(s0, arr[0],arr[2]), MUX(s0, arr[1], arr[3]))
     * where arr[i] refers to the ith input number.
     */
    int switch0 = inputs[4 * num_len];
    int switch1 = inputs[(4 * num_len) + 1];

    int mux_in0[2 * num_len];
    int mux_out0[num_len];
    int mux_in1[2 * num_len];
    int mux_out1[num_len];
    int mux_in2[2 * num_len];

    memcpy(mux_in0, inputs, num_len * sizeof(int));
    memcpy(&mux_in0[num_len], &inputs[2 * num_len], num_len * sizeof(int));
    bitwiseMUX(gc, ctxt, switch0, mux_in0, 2 * num_len, mux_out0);

    memcpy(mux_in1, &inputs[num_len], num_len * sizeof(int));
    memcpy(&mux_in1[num_len], &inputs[3 * num_len], num_len * sizeof(int));
    bitwiseMUX(gc, ctxt, switch0, mux_in1, 2 * num_len, mux_out1);


    memcpy(mux_in2, mux_out0, num_len * sizeof(int));
    memcpy(mux_in2 + num_len, mux_out1, num_len * sizeof(int));
    bitwiseMUX(gc, ctxt, switch1, mux_in2, 2 * num_len, outputs);
}

void circuit_decision_tree_node(garble_circuit *gc, garble_context *ctxt, int num_len, int *inputs, int *outputs)
{
    /* Adds the operation of a single node in a decision tree to the circuit.
     * In particular, this compares the inputs[:num_len-1] to inputs[num_len:]
     * And ands that with a single bit from before
     *
     * int *inputs should be of size (2*num_len) + 1
     * where the first num_len bits are the evaluator's x_i
     * the second num_len bits are the garblers' w_i
     * and the final bit is the output of the decision tree
     * immediately above it. 
     *
     * Returns two values, outputs[0] and outputs[1] where 
     * outputs[0] is the left out-edge (corresponding to true)
     * and outputs[1] is the right out-edge (corresponding to false)
     * of the decision tree node.
     */

    int less_than_out;
    int not_less_than = builder_next_wire(ctxt); // a temporary wire
    outputs[0] = builder_next_wire(ctxt);
    outputs[1] = builder_next_wire(ctxt);

    circuit_signed_less_than(gc, ctxt, 2 * num_len, inputs, &inputs[num_len], &less_than_out);

    gate_AND(gc, ctxt, inputs[2 * num_len], less_than_out, outputs[0]);

    my_not_gate(gc, ctxt, less_than_out, not_less_than);
    gate_AND(gc, ctxt, inputs[2 * num_len], not_less_than, outputs[1]);
}

void build_signed_comparison_circuit(garble_circuit *gc, int num_len)
{
    /* Builds circuit for performing signed comparison. 
     * Form of numbers is dictatd by circuit_signed_less_than, which
     * at the time of writing, is little-endian with a the msb being
     * the signed bit.
     */
    int n = 2 * num_len;
    int m = 1;
    int inputs[n];
    int outputs[m];
    garble_context ctxt;
    
    countToN(inputs, n);

	garble_new(gc, n, m, garble_type);
	builder_start_building(gc, &ctxt);
    circuit_signed_less_than(gc, &ctxt, 2 * num_len, inputs, inputs + num_len, outputs);
	builder_finish_building(gc, &ctxt, outputs);
}

void build_decision_tree_nursery_circuit(garble_circuit *gc, int num_len)
{
    /* Builds a decision tree circuit with the given depth and num_nodes.
     * Each node consists of a less than circuit, comparing an integer 
     *
     * This circuit has four nodes a a depth of four nodes.
     * So each node is also an output.
     */
    uint32_t num_garbler_inputs = 4 * num_len;
    uint32_t num_evaluator_inputs = 4 * num_len;
    int n = 8 * num_len;
    int m = 4;
    int split = n / 2;
    int inputs[n];
    int outputs[m];
    garble_context ctxt;
    

    assert(n == num_garbler_inputs + num_evaluator_inputs);

    countToN(inputs, n);
	garble_new(gc, n, m, garble_type);
	builder_start_building(gc, &ctxt);

    int node_one_out[2];
    int node_two_out[2];
    int node_three_out[2];
    int node_four_out[2];
    
    // node 1
    int dt_inputs[n + 1];
    memcpy(dt_inputs, inputs, num_len * sizeof(int));
    memcpy(dt_inputs + num_len, inputs + split, num_len * sizeof(int));
    dt_inputs[2 * num_len] = wire_one(gc);
    circuit_decision_tree_node(gc, &ctxt, num_len, dt_inputs, node_one_out);

    //node 2
    memcpy(dt_inputs, inputs + num_len, num_len * sizeof(int));
    memcpy(dt_inputs + num_len, inputs + split + num_len, num_len * sizeof(int));
    dt_inputs[2 * num_len] = node_one_out[0];
    circuit_decision_tree_node(gc, &ctxt, num_len, dt_inputs, node_two_out);

    // node 3
    memcpy(dt_inputs, inputs + (2 * num_len), num_len * sizeof(int));
    memcpy(dt_inputs + num_len, inputs + split + (2 * num_len), num_len * sizeof(int));
    dt_inputs[2 * num_len] = node_two_out[0];
    circuit_decision_tree_node(gc, &ctxt, num_len, dt_inputs, node_three_out);

    // node 4
    memcpy(dt_inputs, inputs + (3 * num_len), num_len * sizeof(int));
    memcpy(dt_inputs + num_len, inputs + split + (3 * num_len), num_len * sizeof(int));
    dt_inputs[2 * num_len] = node_three_out[0];
    circuit_decision_tree_node(gc, &ctxt, num_len, dt_inputs, node_four_out);

    outputs[0] = node_one_out[0];
    outputs[1] = node_two_out[0];
    outputs[2] = node_three_out[0];
    outputs[3] = node_four_out[0];

	builder_finish_building(gc, &ctxt, outputs);
}

void build_decision_tree_ecg_circuit(garble_circuit *gc, int num_len)
{
    /* Builds a decision tree circuit with the given depth and num_nodes.
     * Each node consists of a less than circuit, comparing an integer 
     *
     * This circuit has four nodes a a depth of four nodes.
     * So each node is also an output.
     */
    int n = 12 * num_len;
    int m = 4;
    int split = n / 2;
    int inputs[n];
    int outputs[m];
    garble_context ctxt;
    
    countToN(inputs, n);
	garble_new(gc, n, m, garble_type);
	builder_start_building(gc, &ctxt);

    int node_one_out[2];
    int node_two_out[2];
    int node_three_out[2];
    int node_four_out[2];
    int node_five_out[2];
    int node_six_out[2];
    
    //int not_node_one;
    //int not_node_two;

    // node 1
    int dt_inputs[n + 1];
    memcpy(dt_inputs, inputs, num_len * sizeof(int));
    memcpy(dt_inputs + num_len, inputs + split, num_len * sizeof(int));
    dt_inputs[2 * num_len] = wire_one(gc);
    circuit_decision_tree_node(gc, &ctxt, num_len, dt_inputs, node_one_out);

    //node 2
    memcpy(dt_inputs, inputs + num_len, num_len * sizeof(int));
    memcpy(dt_inputs + num_len, inputs + split + num_len, num_len * sizeof(int));
    dt_inputs[2 * num_len] = node_one_out[0];
    circuit_decision_tree_node(gc, &ctxt, num_len, dt_inputs, node_two_out);

    //not_node_two = builder_next_wire(&ctxt);
    //my_not_gate(gc, &ctxt, node_two_out, not_node_two);

    // node 3
    memcpy(dt_inputs, inputs + (2 * num_len), num_len * sizeof(int));
    memcpy(dt_inputs + num_len, inputs + split + (2 * num_len), num_len * sizeof(int));
    dt_inputs[2 * num_len] = node_one_out[1];
    circuit_decision_tree_node(gc, &ctxt, num_len, dt_inputs, node_three_out);

    // node 4
    memcpy(dt_inputs, inputs + (3 * num_len), num_len * sizeof(int));
    memcpy(dt_inputs + num_len, inputs + split + (3 * num_len), num_len * sizeof(int));
    dt_inputs[2 * num_len] = node_two_out[0];
    circuit_decision_tree_node(gc, &ctxt, num_len, dt_inputs, node_four_out);

    // node 5
    memcpy(dt_inputs, inputs + (4 * num_len), num_len * sizeof(int));
    memcpy(dt_inputs + num_len, inputs + split + (4 * num_len), num_len * sizeof(int));
    dt_inputs[2 * num_len] = node_two_out[1];
    circuit_decision_tree_node(gc, &ctxt, num_len, dt_inputs, node_five_out);

    // node 6
    memcpy(dt_inputs, inputs + (5 * num_len), num_len * sizeof(int));
    memcpy(dt_inputs + num_len, inputs + split + (5 * num_len), num_len * sizeof(int));
    dt_inputs[2 * num_len] = node_three_out[0];
    circuit_decision_tree_node(gc, &ctxt, num_len, dt_inputs, node_six_out);

    outputs[0] = node_four_out[0];
    outputs[1] = node_five_out[0];
    outputs[2] = node_six_out[0];
    outputs[3] = node_two_out[1];

	builder_finish_building(gc, &ctxt, outputs);
}

void build_decision_tree_circuit(garble_circuit *gc, uint32_t num_nodes, uint32_t depth, uint32_t num_len)
{
    /* Builds a decision tree circuit with the given depth and num_nodes.
     * Each node consists of a less than circuit, comparing an integer 
     *
     * I am first making the decision tree from figure 2 of raluca et al., assuming 
     * that each operation is a < operation.
     */
    assert(3 == num_nodes);
    assert(2 == depth);

    uint32_t num_garbler_inputs = num_len * 2;
    uint32_t num_evaluator_inputs = num_len * 2;
    int n = 4 * num_len;
    int m = 2; // num_leaf nodes
    int split = n / 2;
    int inputs[n];
    int outputs[m];
    garble_context ctxt;

    assert(n == num_garbler_inputs + num_evaluator_inputs);

    countToN(inputs, n);
	garble_new(gc, n, m, garble_type);
	builder_start_building(gc, &ctxt);

    int node_one_out[2];
    int node_two_out[2];
    int node_three_out[2];
    
    // node 1
    int dt_inputs[n + 1];
    memcpy(dt_inputs, inputs, num_len * sizeof(int));
    memcpy(dt_inputs + num_len, inputs + split, num_len * sizeof(int));
    dt_inputs[2 * num_len] = wire_one(gc);
    circuit_decision_tree_node(gc, &ctxt, num_len, dt_inputs, node_one_out);

    //node 2
    memcpy(dt_inputs, inputs + num_len, num_len * sizeof(int));
    memcpy(dt_inputs + num_len, inputs + split + num_len, num_len * sizeof(int));
    dt_inputs[2 * num_len] = node_one_out[0];
    circuit_decision_tree_node(gc, &ctxt, num_len, dt_inputs, node_two_out);

    // node 3
    memcpy(dt_inputs, inputs + (2 * num_len), num_len * sizeof(int));
    memcpy(dt_inputs + num_len, inputs + split + (2 * num_len), num_len * sizeof(int));
    dt_inputs[2 * num_len] = node_one_out[1];
    circuit_decision_tree_node(gc, &ctxt, num_len, dt_inputs, node_three_out);

    outputs[0] = node_two_out[0];
    outputs[1] = node_three_out[0];

	builder_finish_building(gc, &ctxt, outputs);
}

void build_naive_bayes_circuit(garble_circuit *gc, 
        int num_classes, int vector_size, int domain_size, int num_len)
{
    /*
     * PSEUDOCODE of algorithm:
     * for i in range(num_classes):
     *     t_val[i] = C_inputs[i]
     *     for j in range(vector_size):
     *         int t_val[num_len];
     *         v = client_input[j] # is this correct?
     *         t_val[i] += circuit_select(T_inputs, index=i*j*v)
     * the_argmax = argmax(t_val)
     * return the_argmax
     */

    // sizes in bits, not values
    int client_input_size = vector_size * num_len; 
    int C_size = num_classes * num_len;
    int T_size = num_classes * vector_size * domain_size * num_len;
    int n = client_input_size + C_size + T_size;
    int m = num_len;
    int inputs[n];
    int outputs[m];
    garble_context ctxt;

    countToN(inputs, n);

    int client_inputs[client_input_size];
    int C_inputs[C_size];
    int T_inputs[T_size];

    memcpy(C_inputs, inputs, C_size * sizeof(int));
    memcpy(T_inputs, inputs + C_size, T_size * sizeof(int));
    memcpy(client_inputs, inputs + C_size + T_size , client_input_size * sizeof(int));

	garble_new(gc, n, m, garble_type);
	builder_start_building(gc, &ctxt);
    
    // these nested for loops populate probs with
    // the correct value as defined by naive bayes algo.
    int probs[num_len * num_classes];
    memcpy(probs, C_inputs, C_size * sizeof(int));

    int select_in[T_size + num_len];
    memcpy(select_in, T_inputs, T_size * sizeof(int));
    int carry;

    for (int i = 0; i < num_classes; ++i) {
        int *cur_prob = &probs[i * num_len];
        for (int j = 0; j < vector_size; ++j) {
            int select_out[2 * num_len]; // also used as input to add
            memcpy(select_in + T_size, client_inputs + (j * num_len), num_len * sizeof(int));
            circuit_select(gc, &ctxt, num_len, T_size / num_len, num_len, select_in, select_out);

            int add_out[num_len];
            memcpy(select_out + num_len, cur_prob, num_len * sizeof(int));
		    circuit_add(gc, &ctxt, 2 * num_len, select_out, add_out, &carry);
            memcpy(cur_prob, add_out, num_len * sizeof(int));
        }
    }
    
    // argmax on probs
    int argmax_out[2 * num_len];
    circuit_argmax(gc, &ctxt, probs, argmax_out, num_classes, num_len);
    
    // grab only the index from argmax_out and use it as output
    memcpy(outputs, argmax_out, num_len * sizeof(int));

	builder_finish_building(gc, &ctxt, outputs);
}

void build_not_circuit(garble_circuit *gc) 
{
    // Circuit with only a single AND gate
    int n = 1;
    int m = 1;
    int inputs[n];
    int outputs[m];
    countToN(inputs, n);
    garble_context ctxt;
    garble_new(gc, n, m, garble_type);
    builder_start_building(gc, &ctxt);
    outputs[0] = builder_next_wire(&ctxt);
    my_not_gate(gc, &ctxt, inputs[0], outputs[0]);
    builder_finish_building(gc, &ctxt, outputs);
}

void build_and_circuit(garble_circuit *gc) 
{
    // Circuit with only a single AND gate
    int n = 2;
    int m = 1;
    int inputs[n];
    int outputs[m];
    countToN(inputs, n);
    garble_context ctxt;
    garble_new(gc, n, m, garble_type);
    builder_start_building(gc, &ctxt);
    outputs[0] = builder_next_wire(&ctxt);
    gate_AND(gc, &ctxt, inputs[0], inputs[1], outputs[0]);
    builder_finish_building(gc, &ctxt, outputs);
}

void build_gr0_circuit(garble_circuit *gc, uint32_t n)
{
    int m = 1;
    int input_wires[n];
    int output_wires[m];

    garble_context ctxt;

    countToN(input_wires, n);
	garble_new(gc, n, m, garble_type);
	builder_start_building(gc, &ctxt);

    circuit_gr0(gc, &ctxt, n, input_wires, output_wires);
	builder_finish_building(gc, &ctxt, output_wires);
    printf("built gr0 circuit\n");
}

void build_inner_product_circuit(garble_circuit *gc, uint32_t n, uint32_t num_len)
{
    int input_wires[n];
    int output_wires[num_len];

    garble_context ctxt;

    countToN(input_wires, n);
	garble_new(gc, n, num_len, garble_type);
	builder_start_building(gc, &ctxt);
    circuit_inner_product(gc, &ctxt, n, num_len, input_wires, output_wires);
	builder_finish_building(gc, &ctxt, output_wires);
    printf("build inner product circuit\n");
}

void 
my_circuit_and(garble_circuit *gc, garble_context *ctxt, uint32_t n,
        const int *inputs, int select_wire, int *outputs)  {
    // does and (select_bit, input[i]) for all i = 0 to n
    // n does not include select_bit
    //
    // Loop over inputs and AND each inputs[i] with the select wire
    // such that outputs[i] = inputs[i] & select_wire
    
    // TODO is linear, make tree
    for (uint32_t i = 0; i < n; ++i) {
        outputs[i] = builder_next_wire(ctxt);
	    gate_AND(gc, ctxt, inputs[i], select_wire, outputs[i]);
    }

}

void 
circuit_signed_mult_2s_compl_n(garble_circuit *gc, garble_context *ctxt, uint32_t n,
        int *inputs, int *outputs) 
{
    // http://stackoverflow.com/questions/20793701/how-to-do-two-complement-multiplication-and-division-of-integers
    assert(0 == n % 2);

    uint32_t split = n / 2;
    uint32_t split2 = 2 * split;
    int zero_wire = wire_zero(gc);
    int one_wire = wire_one(gc);
    assert(one_wire && zero_wire);

    int accum[split2]; // holds values for going forward
    assert(accum);
    for (uint32_t i = 0; i < split2; i++) {
        accum[i] = zero_wire;
    }

    for (uint32_t i = 0; i < split; ++i) {
        // and with the appropritiate digit of second number
        int *and_out = calloc(split, sizeof(int));
        int and_select = inputs[n - 1 - i];
        my_circuit_and(gc, ctxt, split, inputs, and_select, and_out);

        // shift the number first number of i bits, and put into shift
        int *add_in = calloc(2 * split2, sizeof(int));
        for (uint32_t j = 0; j < 2 * split2; ++j) {
            add_in[j] = zero_wire;
        }

        memcpy(&add_in[split - i], and_out, split * sizeof(int));
        memcpy(&add_in[split2], accum, split2 * sizeof(int));

        int carry; // can ignore
		circuit_add(gc, ctxt, 2 * split2, add_in, accum, &carry);

        free(and_out);
        and_out = NULL;
        free(add_in);
        add_in = NULL;
    }
    memcpy(outputs, &accum[split], split * sizeof(int));
}

void                                                                                         
circuit_signed_mult_n(garble_circuit *circuit, garble_context *context, uint32_t n,
		int *inputs, int *outputs) 
{
    /* Performs signed multiplication of 
     * two signed little-endian numbers (e.g. 1100 base 2 = -1 base 10)
     *
     * Method: simply multiply the first split-1 bits with normal multiplication,
     * and set the sign bit to the xor of the signed bits of the inputs.
     *
     *
     * Note that a negative 0, e.g. 001 (if split = 3) is not well defined. 
     */

    assert(0 == n % 2);
    uint32_t split = n / 2;
    int mult_inputs[n - 2];
    memcpy(mult_inputs, inputs, (split - 1) * sizeof(int));
    memcpy(&mult_inputs[split - 1], &inputs[split], (split - 1) * sizeof(int));

    // multiply them. Use n - 2 because ignoring the sign of the inputs
    circuit_mult_n(circuit, context, n - 2, mult_inputs, outputs);

    // set the signed bit
    outputs[split - 1] = builder_next_wire(context);
	gate_XOR(circuit, context, inputs[split-1], inputs[n-1], outputs[split - 1]);
}

void 
circuit_mult_n(garble_circuit *gc, garble_context *ctxt, uint32_t n,
        int *inputs, int *outputs) 
{
    /*    10
     *   *10
     *   ----
     *    10
     *     00
     *  = 10 (e.g 1 * 1 = 1 in base 10)
     *
     * https://en.wikipedia.org/wiki/Binary_multiplier
     */

    assert(0 == n % 2);

    uint32_t split = n / 2;
    int carry; // can ignore
    int and_select;
    int zero_wire = wire_zero(gc);
    int and_in[split];
    int add_in[n];
    int partial_product[split];
    int accum[split]; // holds values for going forward

    // do first partial product - populate accum
    and_select = inputs[split];
    memcpy(and_in, inputs, split * sizeof(int));
    my_circuit_and(gc, ctxt, split, and_in, and_select, accum);

    // do the rest of the partial products, adding them into accum
    // at the end of each loop
    for (uint32_t i = 1; i < split; ++i) {
        // get our partial product, as it is called on wikipedia
        // i.e. the and of the number 1 with shifted digits and number 2
        and_select = inputs[split + i];
        memcpy(and_in, inputs, split * sizeof(int));
        my_circuit_and(gc, ctxt, split, and_in, and_select, partial_product);

        // initialize add_in to zero wire
        for (int j = 0; j < split; ++j) {
            add_in[j] = zero_wire;
        }

        // first put partial_product into add_in
        memcpy(&add_in[i], partial_product, (split - i) * sizeof(int));

        // put accum into add_in
        memcpy(&add_in[split], accum, split * sizeof(int));
		circuit_add(gc, ctxt, n, add_in, accum, &carry);
    }

    memcpy(outputs, accum, split * sizeof(int));
}

void circuit_argmax2(garble_circuit *gc, garble_context *ctxt, 
		int *inputs, int *outputs, int num_len) 
{
	/* num_len is the length of each number.
	 *
	 * Inputs should look like idx0 || num0 || idx1 || num1
	 * where the length of idx0, idx1, num0, and num1 is num_len bits
	 */
	assert(inputs && outputs && gc && ctxt);

	// populate idxs and nums by splicing inputs
	int les_ins[2 * num_len];

	memcpy(les_ins, &inputs[num_len], sizeof(int) * num_len); // put num0
	memcpy(&les_ins[num_len], &inputs[3 * num_len], sizeof(int) * num_len); // put num1
	int les_out;

	new_circuit_les(gc, ctxt, num_len * 2, les_ins, &les_out);

	// And mux 'em
	int mux_inputs[2 * num_len];
	memcpy(mux_inputs, inputs, sizeof(int) * num_len);
	memcpy(mux_inputs + num_len, &inputs[2 * num_len], sizeof(int) * num_len);
	bitwiseMUX(gc, ctxt, les_out, inputs, 4 * num_len, outputs);
}

void circuit_argmax4(garble_circuit *gc, garble_context *ctxt,
		int *inputs, int *outputs, int num_len) 
{
	assert(inputs && outputs && gc && ctxt);

	// argmax first two numbers
	int out[4 * num_len]; // will hold output of first two argmaxes
	circuit_argmax2(gc, ctxt, inputs, out, num_len);

	// argmax second two
	circuit_argmax2(gc, ctxt, inputs + (4 * num_len), out + (2 * num_len), num_len);

	// argmax their outputs
	circuit_argmax2(gc, ctxt, out, outputs, num_len);
}

void circuit_argmax(garble_circuit *gc, garble_context *ctxt, 
        int *inputs, int *outputs, int input_array_size, int num_len) 
{
    /* Performs argmax on arbitrary size arrays
     *
     *
     * Inputs should have length array_size * num_len
     *
     * Idea: turn array into a binary tree. 
     * take argmax of every other input, and move the output 
     * up the tree. very similar to how other tree-based circuits 
     * are constructed here.
     *
	 * Inputs to circuit_argmax2 should look like idx0 || num0 || idx1 || num1 || ...
	 * where the length of idx0, idx1, num0, and num1 is num_len bits
     */

    // Removed assertoin because overflows for large num_len
    //int max_num_len_val = pow(2, num_len) - 1; // max value that num_len bits can represent
    //assert(input_array_size <= max_num_len_val);

    // Make a new array called idx_val_inputs which puts the index
    // of element of the list before each value.
    // e.g. if inputs looks like [1,0,0,1] where num_len is 2, 
    // then idx_val_inputs will look like [0, 0, 1, 0, 1, 0, 0, 1]
    // where 00 and 10 are the insert indices, and 10, 01 are the values.
    int zero_wire = wire_zero(gc);
    int one_wire = wire_one(gc);
    int idx_val_len = 2 * num_len; // the length of the value plus the index
    int idx_val_inputs[input_array_size * idx_val_len];

    for (int i = 0; i < input_array_size; ++i) {
        bool bin[num_len];
        convertToBinary(i, bin, num_len);

        // convert 1 to fix_one_wire and 0 to fix_zero_wire
        int bin_wires[num_len];
        for (int j = 0; j < num_len; ++j) {
            if (bin[j] == 0) {
                bin_wires[j] = zero_wire;
            } else {
                bin_wires[j] = one_wire;
            }
        }

        // copy bin_wires into idx_val_inputs
        memcpy(idx_val_inputs + (i * idx_val_len), bin_wires, num_len * sizeof(int));

        // copy value into idx_val_inputs
        memcpy(idx_val_inputs + (i * idx_val_len) + num_len, 
               inputs + (i * num_len),
               num_len * sizeof(int));
    }
        
    // prepare values for tree
    int array_size = pow(2, ceil(log2(input_array_size)));
    assert(input_array_size <= array_size);

    int tree_depth = floor(log2(array_size));
    int tree_vals[array_size * idx_val_len];

    memcpy(tree_vals, idx_val_inputs, idx_val_len * input_array_size * sizeof(int));
    for (int i = idx_val_len * input_array_size; i < idx_val_len * array_size; ++i) {
        tree_vals[i] = -1;
    }


    // loop over tree from bottom to the top
    for (int tree_level = 0; tree_level < tree_depth; ++tree_level) {
        int num_nodes = pow(2, tree_depth - tree_level);
        assert(num_nodes % 2 == 0);

        int new_tree_size = idx_val_len * (num_nodes / 2);
        int new_tree_vals[new_tree_size]; // values for next level of tree
        assert(new_tree_vals);
        for (int i = 0; i < new_tree_size; i++) {
            new_tree_vals[i] = -1;
        }
        for (int node = 0; node < num_nodes; node+=2) {
            if (tree_vals[idx_val_len * node] == -1 && tree_vals[idx_val_len * (node + 1)]) {
                // both values are negative one, so leave new_tree_vals as -1
                // do nothing
            } else if (tree_vals[idx_val_len * (node + 1)] == -1) {
                // only one value is -1, so set new_tree_vals equal to 
                // the other value
                memcpy(new_tree_vals + (idx_val_len * (node / 2)), 
                        tree_vals + (idx_val_len * node), idx_val_len * sizeof(int));
            } else {
                // argmax these guys
                circuit_argmax2(
                        gc, 
                        ctxt, 
                        &tree_vals[idx_val_len * node], 
                        &new_tree_vals[idx_val_len * (node / 2)], 
                        num_len);
            }
        }
        memcpy(tree_vals, new_tree_vals, new_tree_size * sizeof(int));
    }
    memcpy(outputs, tree_vals, idx_val_len * sizeof(int)); 
}


void build_argmax_circuit(garble_circuit *gc, int n, int num_len) 
{
    assert(0 == n % num_len);
    int input_array_size = n / num_len;

    int m = num_len;
    int inputs[n];
    int outputs[m];
    garble_context ctxt;
    
    countToN(inputs, n);

	garble_new(gc, n, m, garble_type);
	builder_start_building(gc, &ctxt);

    circuit_argmax(gc, &ctxt, inputs, outputs, input_array_size, num_len);
	builder_finish_building(gc, &ctxt, outputs);
}

void build_add_circuit(garble_circuit *gc, int num_len) 
{
    int n = 2 * num_len;
    int m = num_len;
    int inputs[n];
    int outputs[m];
    garble_context ctxt;
    
    countToN(inputs, n);

	garble_new(gc, n, m, garble_type);
	builder_start_building(gc, &ctxt);

    int carry;
    circuit_add(gc, &ctxt, 2 * num_len, inputs, outputs, &carry);
	builder_finish_building(gc, &ctxt, outputs);
}

void build_select_circuit(garble_circuit *gc, int num_len, int input_array_size) 
{
    int n = (input_array_size * num_len) + num_len;
    int m = num_len;
    int inputs[n];
    int outputs[m];
    garble_context ctxt;
    
    countToN(inputs, n);

	garble_new(gc, n, m, garble_type);
	builder_start_building(gc, &ctxt);

    circuit_select(gc, &ctxt, num_len, input_array_size, num_len, inputs, outputs);

	builder_finish_building(gc, &ctxt, outputs);
}

void new_circuit_les(garble_circuit *gc, garble_context *ctxt, int n,
		int *inputs, int *output) 
{
    /* Less than circuit
     * Modified from greater than circuit in KSS09 Figure 5 and 6
     *
     * Outputs 1 if x < y else 0
     * Outputs 1 if inputs[0:split-1] < inputs[split:n-1], else 0
     *
     * Inputs are unsigned little-endian 
     */

    int split = n / 2;
    int carry = wire_zero(gc);
    for (int i = 0; i < split; ++i) {
        int x_i = inputs[i];
        int y_i = inputs[split + i];
        int xor1 = builder_next_wire(ctxt);
        int xor2 = builder_next_wire(ctxt);
        int and = builder_next_wire(ctxt);
        int new_carry = builder_next_wire(ctxt);


        gate_XOR(gc, ctxt, x_i, carry, xor1);
        gate_XOR(gc, ctxt, y_i, carry, xor2);
        gate_AND(gc, ctxt, xor1, xor2, and);
        gate_XOR(gc, ctxt, y_i, and, new_carry);
        carry = new_carry;
    }
    *output = carry;
}

void
new_circuit_mux21(garble_circuit *gc, garble_context *ctxt, 
		int theSwitch, int input0, int input1, int *output)
{
	/* MUX21 circuit. 
	 * It takes three inputs, a switch, and two bits. 
	 * If the switch is 0, then it ouptut input0. 
	 * If the switch is 1, then it outputs input1.
     *
     * Taken from KSS09 figure 10
	 */

    int xor1 = builder_next_wire(ctxt);
    int and = builder_next_wire(ctxt);
    int xor2 = builder_next_wire(ctxt);

    gate_XOR(gc, ctxt, input0, input1, xor1);
    gate_AND(gc, ctxt, xor1, theSwitch, and);
    gate_XOR(gc, ctxt, input0, and, xor2);
    *output = xor2;
}

	int
countToN(int *a, int n)
{
	for (int i = 0; i < n; i++)
		a[i] = i;
	return 0;
}

bool isFinalCircuitType(CircuitType type) 
{
	if (type == AES_FINAL_ROUND) 
		return true;
	return false;
}


void buildLinearCircuit(garble_circuit *gc, int n, int num_len) 
{
    /* Builds a circuit that performs linear classification.
     * That is, it takes the dot product of x and w, where x is in the iput
     * and w is the model, and outputs 1 if (<x,w> > 1) and 0 otherwise.
     */
    int m = 1;
    int input_wires[n];
	int output_wire[1];
    int ip_output_size = n / (num_len * 2);
	int ip_output_wires[ip_output_size];
	garble_context ctxt;

	countToN(input_wires, n);

	garble_new(gc, n, m, garble_type);
	builder_start_building(gc, &ctxt);

    circuit_inner_product(gc, &ctxt, n, num_len, input_wires, ip_output_wires);
    circuit_gr0(gc, &ctxt, ip_output_size, ip_output_wires, output_wire);

	builder_finish_building(gc, &ctxt, output_wire);
}



void buildHyperCircuit(garble_circuit *gc) 
{
    int n = 8;
    int num_len = 2;
    int m = 2;
    int input_wires[n];
	int output_wires[m];
	garble_context ctxt;

	countToN(input_wires, n);

	garble_new(gc, n, m, garble_type);
	builder_start_building(gc, &ctxt);

    circuit_inner_product(gc, &ctxt, n, num_len, input_wires, output_wires);


	builder_finish_building(gc, &ctxt, output_wires);
}

void
buildLevenshteinCircuit(garble_circuit *gc, int l, int sigma)
{
	/* The goal of the levenshtein algorithm is
	 * populate the l+1 by l+1 D matrix. 
	 * D[i][j] points to an integer address. 
	 * At that address is an DIntSize-bit number representing the distance between
	 * a[0:i] and b[0:j] (python syntax)
	 *
	 * If l = 2
	 * e.g. D[1][2] = 0x6212e0
	 * *0x6212e0 = {0,1} = the wire indices
	 *
	 * For a concrete understanding of the algorithm implemented, 
	 * see extra_scripts/leven.py
	 */

	/* get number of bits needed to represent our numbers.
	 * The input strings can be at most l distance apart,
	 * since they are of length l, so we need floor(log_2(l)+1) bits
	 * to express the number
	 */
	int DIntSize = (int) floor(log2(l)) + 1;
	int inputsDevotedToD = DIntSize * (l+1);

	int n = inputsDevotedToD + (2 * sigma * l);
	int m = DIntSize;
	int core_n = (3 * DIntSize) + (2 * sigma);
	int inputWires[n];
	countToN(inputWires, n);

	garble_context gctxt;
	garble_new(gc, n, m, garble_type);
	builder_start_building(gc, &gctxt);

	///* Populate D's 0th row and column with wire indices from inputs*/
	int D[l+1][l+1][DIntSize];
	for (int i = 0; i < l+1; i++) {
		for (int j = 0; j < l+1; j++) {
			//D[i][j] = allocate_ints(DIntSize);
			if (i == 0) {
				memcpy(D[0][j], inputWires + (j*DIntSize), sizeof(int) * DIntSize);
			} else if (j == 0) {
				memcpy(D[i][0], inputWires + (i*DIntSize), sizeof(int) * DIntSize);
			}
		}
	}

	/* Populate a and b (the input strings) wire indices */
	int a[l * sigma];
	int b[l * sigma];
	memcpy(a, inputWires + inputsDevotedToD, l * sigma * sizeof(int));
	memcpy(b, inputWires + inputsDevotedToD + (l * sigma), l * sigma * sizeof(int));

	/* dynamically add levenshtein core circuits */
	for (int i = 1; i < l + 1; i++) {
		for (int j = 1; j < l + 1; j++) {
			int coreInputWires[core_n];
			int p = 0;
			memcpy(coreInputWires + p, D[i-1][j-1], sizeof(int) * DIntSize);
			p += DIntSize;
			memcpy(coreInputWires + p, D[i][j-1], sizeof(int) * DIntSize);
			p += DIntSize;
			memcpy(coreInputWires + p, D[i-1][j], sizeof(int) * DIntSize);
			p += DIntSize;
			memcpy(coreInputWires + p, &a[(i-1)*sigma], sizeof(int) * sigma);
			p += sigma;
			memcpy(coreInputWires + p, &b[(j-1)*sigma], sizeof(int) * sigma);
			p += sigma;
			assert(p == core_n);


			int coreOutputWires[DIntSize];
			addLevenshteinCoreCircuit(gc, &gctxt, l, sigma, coreInputWires, coreOutputWires);
			/*printf("coreInputWires: (i=%d,j=%d) (%d %d) (%d %d) (%d %d) (%d %d) (%d %d) -> (%d %d)\n",*/
			/*i,*/
			/*j,*/
			/*coreInputWires[0],*/
			/*coreInputWires[1],*/
			/*coreInputWires[2],*/
			/*coreInputWires[3],*/
			/*coreInputWires[4],*/
			/*coreInputWires[5],*/
			/*coreInputWires[6],*/
			/*coreInputWires[7],*/
			/*coreInputWires[8],*/
			/*coreInputWires[9],*/
			/*coreOutputWires[0],*/
			/*coreOutputWires[1]);*/

			// Save coreOutputWires to D[i][j] 
			memcpy(D[i][j], coreOutputWires, sizeof(int) * DIntSize);
		}
	}
	int output_wires[m];
	memcpy(output_wires, D[l][l], sizeof(int) * DIntSize);
	builder_finish_building(gc, &gctxt, output_wires);
}

int 
INCCircuitWithSwitch(garble_circuit *gc, garble_context *ctxt,
		int the_switch, int n, int *inputs, int *outputs) {
	/* n does not include the switch. The size of the number */

	for (int i = 0; i < n; i++) {
		outputs[i] = builder_next_wire(ctxt);
	}
	int carry = builder_next_wire(ctxt);
	gate_XOR(gc, ctxt, the_switch, inputs[0], outputs[0]);
	gate_AND(gc, ctxt, the_switch, inputs[0], carry);

	/* do first case */
	int not_switch = builder_next_wire(ctxt);
	gate_NOT(gc, ctxt, the_switch, not_switch);
	for (int i = 1; i < n; i++) {
		/* cout and(xor(x,c),s) */
		int xor_out = builder_next_wire(ctxt);
		int and0 = builder_next_wire(ctxt);
		int and1 = builder_next_wire(ctxt);
		gate_XOR(gc, ctxt, carry, inputs[i], xor_out);
		gate_AND(gc, ctxt, xor_out, the_switch, and0);

		gate_AND(gc, ctxt, not_switch, inputs[i], and1);
		gate_OR(gc, ctxt, and0, and1, outputs[i]);

		/* carry and(nand(c,x),s)*/
		int and_out = builder_next_wire(ctxt);
		gate_AND(gc, ctxt, carry, inputs[i], and_out);
		carry = builder_next_wire(ctxt);
		gate_AND(gc, ctxt, and_out, the_switch, carry);
	}
	return 0;
}

	static int
TCircuit(garble_circuit *gc, garble_context *gctxt, const int *inp0, const int *inp1, int ninputs)
{
	/* Perfroms "T" which equal 1 if and only if inp0 == inp1 */
	/* returns the output wire */
	assert(ninputs % 2 == 0 && "doesnt support other alphabet sizes.. yet");
	int split = ninputs / 2;
	int xor_output[split];

	for (int i = 0; i < split; i++) {
		xor_output[i] = builder_next_wire(gctxt);
		gate_XOR(gc, gctxt, inp0[i], inp1[i], xor_output[i]);
	}
	int T_output;
	circuit_or(gc, gctxt, split, xor_output, &T_output);
	return T_output;
}

	void
addLevenshteinCoreCircuit(garble_circuit *gc, garble_context *gctxt, 
		int l, int sigma, int *inputWires, int *outputWires) 
{
	int DIntSize = (int) floor(log2(l)) + 1;
	int D_minus_minus[DIntSize]; /*D[i-1][j-1] */
	int D_minus_same[DIntSize]; /* D[1-1][j] */
	int D_same_minus[DIntSize]; /* D[i][j-1] */
	int symbol0[sigma];
	int symbol1[sigma];

	/* arrayPopulateRange is inclusive on start and exclusive on end */
	memcpy(D_minus_minus, inputWires, sizeof(int) * DIntSize);
	memcpy(D_minus_same, inputWires + DIntSize, sizeof(int) * DIntSize);
	memcpy(D_same_minus, inputWires + 2*DIntSize, sizeof(int) * DIntSize);
	memcpy(symbol0, inputWires + (3*DIntSize), sizeof(int) * sigma);
	memcpy(symbol1, inputWires + (3*DIntSize) + sigma, sizeof(int) * sigma);

	/* First MIN circuit :MIN(D_minus_same, D_same_minus) */
	int min_inputs[2 * DIntSize];
	memcpy(min_inputs + 0, D_minus_same, sizeof(int) * DIntSize);
	memcpy(min_inputs + DIntSize, D_same_minus, sizeof(int) * DIntSize);
	int min_outputs[DIntSize]; 
	circuit_min(gc, gctxt, 2 * DIntSize, min_inputs, min_outputs);

	/* Second MIN Circuit: uses input from first min cricuit and D_minus_minus */
	memcpy(min_inputs, min_outputs, sizeof(int) * DIntSize);
	memcpy(min_inputs + DIntSize,  D_minus_minus, sizeof(int) * DIntSize);
	int min_outputs2[DIntSize + 1]; 
	MINCircuitWithLEQOutput(gc, gctxt, 2 * DIntSize, min_inputs, min_outputs2); 

	int T_output = TCircuit(gc, gctxt, symbol0, symbol1, 2*sigma);

	/* 2-1 MUX(switch = determined by secon min, 1, T)*/
	int mux_switch = min_outputs2[DIntSize];

	int fixed_one_wire = wire_one(gc);

	int mux_output;
	circuit_mux21(gc, gctxt, mux_switch, fixed_one_wire, T_output, &mux_output);

	int final[DIntSize];
	INCCircuitWithSwitch(gc, gctxt, mux_output, DIntSize, min_outputs2, final);

	memcpy(outputWires, final, sizeof(int) * DIntSize);
}

int myLEQCircuit(garble_circuit *gc, garble_context *ctxt, int n,
		int *inputs, int *outputs)
{

	int les, eq;
	int ret = builder_next_wire(ctxt);

	circuit_les(gc, ctxt, n, inputs, &les);
	circuit_equ(gc, ctxt, n, inputs, &eq);
	gate_OR(gc, ctxt, les, eq, ret);
	outputs[0] = ret;
	return 0;
}

int MINCircuitWithLEQOutput(garble_circuit *gc, garble_context *garblingContext, int n,
		int* inputs, int* outputs) 
{
	/* Essentially copied from JustGarble/src/circuits.c.
	 * Different from their MIN circuit because it has output size equal to n/2 + 1
	 * where that last bit indicates the output of the LEQ circuit,
	 * e.g. outputs[2/n + 1] = 1 iff input0 <= input1
	 */
	int i;
	int lesOutput;
	int notOutput = builder_next_wire(garblingContext);
	myLEQCircuit(gc, garblingContext, n, inputs, &lesOutput);
	gate_NOT(gc, garblingContext, lesOutput, notOutput);
	int split = n / 2;
	for (i = 0; i < split; i++) {
		circuit_mux21(gc, garblingContext, lesOutput, inputs[i], inputs[split + i], outputs+i);
	}

	outputs[split] = lesOutput;
	return 0;
}

	void
buildANDCircuit(garble_circuit *gc, int n, int nlayers)
{
	garble_context ctxt;
	int wire;
	int wires[n];

	countToN(wires, n);

	garble_new(gc, n, n, garble_type);
	builder_start_building(gc, &ctxt);

	for (int i = 0; i < nlayers; ++i) {
		for (int j = 0; j < n; j += 2) {
			wire = builder_next_wire(&ctxt);
			gate_AND(gc, &ctxt, wires[j], wires[j+1], wire);
			wires[j] = wires[j+1] = wire;
		}
	}

	builder_finish_building(gc, &ctxt, wires);
}

void AddAESCircuit(garble_circuit *gc, garble_context *garblingContext, int numAESRounds, 
		int *inputWires, int *outputWires) 
{
	/* Adds AES to the circuit
	 * :param inputWires has size 128 * numAESRounds + 128
	 */

	int addKeyInputs[256];
	int addKeyOutputs[128];
	int subBytesOutputs[128];
	int shiftRowsOutputs[128];
	int mixColumnOutputs[128];

	memcpy(addKeyInputs, inputWires, sizeof(addKeyInputs));

	for (int round = 0; round < numAESRounds; round++) {
		aescircuit_add_round_key(gc, garblingContext, addKeyInputs, addKeyOutputs);

		for (int j = 0; j < 16; j++) {
			aescircuit_sub_bytes(gc, garblingContext, addKeyOutputs + (8*j), subBytesOutputs + (8*j));
		}

		aescircuit_shift_rows(subBytesOutputs, shiftRowsOutputs);

		if (round != numAESRounds-1) { /*if not final round */
			for (int j = 0; j < 4; j++) {
				aescircuit_mix_columns(gc, garblingContext, shiftRowsOutputs + j * 32, mixColumnOutputs + 32 * j);
			}

			memcpy(addKeyInputs, mixColumnOutputs, sizeof(mixColumnOutputs));
			memcpy(addKeyInputs + 128, inputWires + (round + 2) * 128, sizeof(int)*128);
		} 
	}

	for (int i = 0; i < 128; i++) {
		outputWires[i] = shiftRowsOutputs[i];
	}
}

	void 
buildCBCFullCircuit (garble_circuit *gc, int num_message_blocks, int num_aes_rounds, block *delta) 
{
	int n = (num_message_blocks * 128) + (num_message_blocks * num_aes_rounds * 128) + 128;
	int m = num_message_blocks*128;
	int num_evaluator_inputs = 128 * num_message_blocks;
	int num_garbler_inputs = n - num_evaluator_inputs;

	/* garbler_inputs and evaluator_inputs are the wires to 
	 * which the garbler's and evaluators inputs should go */
	int *garbler_inputs = (int*) malloc(sizeof(int) * num_garbler_inputs);
	int *evaluator_inputs = (int*) malloc(sizeof(int) * num_evaluator_inputs);

	countToN(evaluator_inputs, num_evaluator_inputs);
	for (int i = 0; i < num_garbler_inputs ; i++)
		garbler_inputs[i] = i + num_evaluator_inputs;

	int xorIn[256] = {0};
	int xorOut[128] = {0};
	int aesOut[128] = {0};
	int *aesIn = (int*) malloc(sizeof(int) * (128 * (num_aes_rounds + 1)));
	int *outputWires = (int*) malloc(sizeof(int) * m);
	assert(aesIn && outputWires);

	garble_new(gc, n, m, garble_type);
	garble_context gc_context;
	builder_start_building(gc, &gc_context);

	countToN(xorIn, 256);
	circuit_xor(gc, &gc_context, 256, xorIn, xorOut);

	int output_idx = 0;
	int garbler_input_idx = 128;
	int evaluator_input_idx = 128;

	memcpy(aesIn, xorOut, sizeof(xorOut)); // gets the out wires from xor circuit
	memcpy(aesIn + 128, garbler_inputs + garbler_input_idx, num_aes_rounds * 128);
	garbler_input_idx += 128*num_aes_rounds;

	AddAESCircuit(gc, &gc_context, num_aes_rounds, aesIn, aesOut);
	memcpy(outputWires + output_idx, aesOut, sizeof(aesOut));
	output_idx += 128;

	for (int i = 0; i < num_message_blocks - 1; i++) {
		/* xor previous round output with new message block */
		memcpy(xorIn, aesOut, sizeof(aesOut));
		memcpy(xorIn + 128, evaluator_inputs + evaluator_input_idx, 128);
		evaluator_input_idx += 128;

		circuit_xor(gc, &gc_context, 256, xorIn, xorOut);

		/* AES the output of the xor, and add in the key */
		memcpy(aesIn, xorOut, sizeof(xorOut)); // gets the out wires from xor circuit
		memcpy(aesIn + 128, garbler_inputs + garbler_input_idx, num_aes_rounds * 128);
		garbler_input_idx += 128 * num_aes_rounds;

		AddAESCircuit(gc, &gc_context, num_aes_rounds, aesIn, aesOut);

		memcpy(outputWires + output_idx, aesOut, sizeof(aesOut));
		output_idx += 128;
	}
	assert(garbler_input_idx == num_garbler_inputs);
	assert(evaluator_input_idx == num_evaluator_inputs);
	assert(output_idx == m);
	builder_finish_building(gc, &gc_context, outputWires);
}

	void
buildAdderCircuit(garble_circuit *gc) 
{
	int n = 2; // number of inputs 
	int m = 2; // number of outputs

	int *inputs = (int *) malloc(sizeof(int) * n);
	countToN(inputs, n);
	int* outputs = (int*) malloc(sizeof(int) * m);

	garble_context gc_context;

	garble_new(gc, n, m, garble_type);
	builder_start_building(gc, &gc_context);
	circuit_add22(gc, &gc_context, inputs, outputs);
	builder_finish_building(gc, &gc_context, outputs);

	free(inputs);
	free(outputs);
}

	void 
buildXORCircuit(garble_circuit *gc, block *delta) 
{
	garble_context garblingContext;
	int n = 256;
	int m = 128;
	int inp[n];
	int outs[m];
	countToN(inp, n);

	garble_new(gc, n, m, garble_type);
	builder_start_building(gc, &garblingContext);
	circuit_xor(gc, &garblingContext, 256, inp, outs);
	builder_finish_building(gc, &garblingContext, outs);
}

	void
buildAESRoundComponentCircuit(garble_circuit *gc, bool isFinalRound, block* delta) 
{
	garble_context garblingContext;
	int n1 = 128; // size of key
	int n = 256; // tot size of input: prev value is 128 bits + key size 128 bits
	int m = 128;
	int inp[n];
	countToN(inp, n);
	int prevAndKey[n];
	int keyOutputs[n1];
	int subBytesOutputs[n1];
	int shiftRowsOutputs[n1];
	int mixColumnOutputs[n1];

	garble_new(gc, n, m, garble_type);
	builder_start_building(gc, &garblingContext);
	countToN(prevAndKey, 256); 

	// first 128 bits of prevAndKey are prev value
	// second 128 bits of prevAndKey are the new key
	// addRoundKey xors them together into keyOutputs
	aescircuit_add_round_key(gc, &garblingContext, prevAndKey, keyOutputs);

	for (int i = 0; i < 16; i++) {
		aescircuit_sub_bytes(gc, &garblingContext, keyOutputs + (8*i), subBytesOutputs + (8*i));
	}

	aescircuit_shift_rows(subBytesOutputs, shiftRowsOutputs);

	if (!isFinalRound) { 
		for (int i = 0; i < 4; i++) { 
			// fixed justGarble bug in their construction
			aescircuit_mix_columns(gc, &garblingContext, shiftRowsOutputs + i * 32, mixColumnOutputs + 32 * i);
		}
		// output wires are stored in mixColumnOutputs
		builder_finish_building(gc, &garblingContext, mixColumnOutputs);
	} else {
		builder_finish_building(gc, &garblingContext, shiftRowsOutputs);
	}
}

void buildAESCircuit(garble_circuit *gc)
{
	garble_context garblingContext;

	int roundLimit = 10;
	int n = 128 * (roundLimit + 1);
	int m = 128;
	int* final;
	int inp[n];
	countToN(inp, n);
	int addKeyInputs[n * (roundLimit + 1)];
	int addKeyOutputs[n];
	int subBytesOutputs[n];
	int shiftRowsOutputs[n];
	int mixColumnOutputs[n];
	int i;

	garble_new(gc, n, m, garble_type);
	builder_start_building(gc, &garblingContext);

	countToN(addKeyInputs, 256);

	for (int round = 0; round < roundLimit; round++) {

		aescircuit_add_round_key(gc, &garblingContext, addKeyInputs,
				addKeyOutputs);

		for (i = 0; i < 16; i++) {
			aescircuit_sub_bytes(gc, &garblingContext, addKeyOutputs + 8 * i,
					subBytesOutputs + 8 * i);
		}

		aescircuit_shift_rows(subBytesOutputs, shiftRowsOutputs);

		if (round != roundLimit - 1) {
			for (i = 0; i < 4; i++) {
				aescircuit_mix_columns(gc, &garblingContext,
						shiftRowsOutputs + i * 32, mixColumnOutputs + 32 * i);
			}
		}

		for (i = 0; i < 128; i++) {
			addKeyInputs[i] = mixColumnOutputs[i];
			addKeyInputs[i + 128] = (round + 2) * 128 + i;
		}
	}
	final = shiftRowsOutputs;
	builder_finish_building(gc, &garblingContext, final);
}

