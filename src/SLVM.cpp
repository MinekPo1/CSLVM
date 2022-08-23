#include <GLFW/glfw3.h>
#include <map>
#include <stdio.h>
#include <string>
#include <stack>
#include <vector>
#include "pre-parser.cpp"
#include <queue>

// this allows an easy hop to data types with more capacity
#define num_t float
#define addr_t int

addr_t MEMORY_SIZE = 0x10000;

struct MemoryCell {
	union {
		num_t n;
		std::string * s;
	} value;
	bool is_num_t;

	num_t get_num() {
		if (is_num_t)
			return value.n;
		// convert string to num_t
		// note: we can loose some accuracy
		//       since stof returns float
		return std::stof(*value.s);
	}

	std::string get_string() {
		if (is_num_t)
			return std::to_string(value.n);
		return *value.s;
	}

	void set_num(num_t n) {
		value.n = n;
		is_num_t = true;
	}

	void set_string(std::string s) {
		value.s = new std::string(s);
		is_num_t = false;
	}

	MemoryCell() {
		is_num_t = true;
		value.n = 0;
	}

	~MemoryCell() {
		if (!is_num_t)
			delete value.s;
	}
};

enum GraphicInstructions{
	GI_P_PX,
	GI_P_LN,
	GI_P_REC,
	GI_P_TXT,
	GI_GOTO,
	GI_SET_CL
};

struct GraphicInstruction {
	GraphicInstructions instruction;
	union Data {
		struct {
			num_t x;
			num_t y;
		} D_GI_P_PX;
		struct {
			num_t x0;
			num_t y0;
			num_t x1;
			num_t y1;
		} D_GI_P_LN;
		struct {
			num_t x;
			num_t y;
			num_t w;
			num_t h;
		} D_GI_P_REC;
		struct {
			std::string * text;
		} D_GI_P_TXT;
		struct {
			num_t x;
			num_t y;
		} D_GI_GOTO;
		struct {
			num_t cl;
		} D_GI_S_CL;
	} data;
};

struct SLVM_state{
	                            MemoryCell* memory;
	                            MemoryCell  accumulator;
	                                addr_t  instruction_pointer;
	                    std::stack<addr_t>  call_stack;
	         std::map<std::string, addr_t>  lookup_table;
	std::vector<std::pair<addr_t, addr_t>>  free_chunks; // <start, length>
	                                  bool  running;
	        std::queue<GraphicInstruction>  graphic_queue;
	                std::stack<MemoryCell>  data_stack;

	SLVM_state() {
		memory = new MemoryCell[MEMORY_SIZE];
		free_chunks.push_back(std::make_pair(0, MEMORY_SIZE));
		instruction_pointer = 0;
		running = true;
	}

	~SLVM_state() {
		delete[] memory;
	}

	addr_t allocate_memory(addr_t size) {
		auto it = free_chunks.begin();
		while (it != free_chunks.end()) {
			if (it->second == size) {
				instruction_pointer = it->first;
				addr_t addr = it->first;
				free_chunks.erase(it);
				return addr;
			}
			if (it->second > size) {
				addr_t addr = it->first;
				it->first += size;
				it->second -= size;
				return addr;
			}
			it++;
		}
		printf("Error: out of memory\n");
		running = false;
		return 0;
	}

	void deallocate_memory(addr_t addr, addr_t size) {
		auto it = free_chunks.begin();
		while (it != free_chunks.end()) {
			if (it->first < addr and it->first + it->second > addr + size) {
				// fully contained
				// do nothing lol
				return;
			}
			if (it->first < addr and it->first + it->second > addr) {
				// partially contained
				it->second += it->first + it->second - addr;
				if (it + 1 != free_chunks.end()) {
					if (it->first + it->second <= (it + 1)->first) {
						it->second += (it + 1)->second;
						free_chunks.erase(it + 1);
					}
				}
				return;
			}
			if (it->first + it->second == addr) {
				it->first;
				it->second += size;
				// check if we can merge with the next free chunk
				if (it + 1 != free_chunks.end()) {
					if (it->first + it->second <= (it + 1)->first) {
						it->second += (it + 1)->second;
						free_chunks.erase(it + 1);
					}
				}
				return;
			}
			if (it->first > addr) {
				it = free_chunks.insert(it, { addr, size });
				return;
			}
			it++;
		}

		std::pair<addr_t, addr_t> p = { addr, size };
		// insert, so that the v remains sorted
		it = free_chunks.begin();
		while (it != free_chunks.end() and it->first < addr) {
			it++;
		}
		free_chunks.insert(it, p);
		// check if we can merge with the next free chunk
		if (it + 1 != free_chunks.end()) {
			if (it->first + it->second <= (it + 1)->first) {
				it->second += (it + 1)->second;
				free_chunks.erase(it + 1);
			}
		}
	}

	void process(std::string code[], Instruction instructions[]);

	addr_t get_var(std::string name) {
		if (lookup_table.find(name) == lookup_table.end()) {
			// create new variable
			addr_t addr = allocate_memory(1);
			lookup_table[name] = addr;
		}
		return lookup_table[name];
	}
};

namespace Instructions {
	Instruction last_impl = I_putRect;
	// why are the function arguments r padded?
	// because no one stopped me.
	void fI_unknown                   (SLVM_state * state, std::string code[]) {
		// this will never be called
	}
	void fI_ldi                       (SLVM_state * state, std::string code[]) {
		state->accumulator.set_string(code[state->instruction_pointer + 1]);
		state->instruction_pointer ++; // it will be incremented by the caller a second time
	}
	void fI_loadAtVar                 (SLVM_state * state, std::string code[]) {
		addr_t addr = state->get_var(code[state->instruction_pointer + 1]);
		state->accumulator = state->memory[addr];
		state->instruction_pointer ++; // it will be incremented by the caller a second time
	}
	void fI_storeAtVar                (SLVM_state * state, std::string code[]) {
		addr_t addr = state->get_var(code[state->instruction_pointer + 1]);
		state->memory[addr] = state->accumulator;
		state->instruction_pointer ++; // it will be incremented by the caller a second time
	}
	void fI_jts                       (SLVM_state * state, std::string code[]) {
		// jump to stack
		addr_t addr = atoi(code[state->instruction_pointer + 1].c_str());
		state->call_stack.push(state->instruction_pointer);
		state->instruction_pointer = addr;
		state->instruction_pointer ++;
	}
	void fI_ret                       (SLVM_state * state, std::string code[]) {
		// return from stack
		state->instruction_pointer = state->call_stack.top();
		state->call_stack.pop();
	}
	void fI_addWithVar                (SLVM_state * state, std::string code[]) {
		addr_t addr = state->get_var(code[state->instruction_pointer + 1]);
		state->accumulator.set_num(state->accumulator.get_num() + state->memory[addr].get_num());
		state->instruction_pointer ++; // it will be incremented by the caller a second time
	}
	void fI_subWithVar                (SLVM_state * state, std::string code[]) {
		addr_t addr = state->get_var(code[state->instruction_pointer + 1]);
		state->accumulator.set_num(state->accumulator.get_num() - state->memory[addr].get_num());
		state->instruction_pointer ++; // it will be incremented by the caller a second time
	}
	void fI_mulWithVar                (SLVM_state * state, std::string code[]) {
		addr_t addr = state->get_var(code[state->instruction_pointer + 1]);
		state->accumulator.set_num(state->accumulator.get_num() * state->memory[addr].get_num());
		state->instruction_pointer ++; // it will be incremented by the caller a second time
	}
	void fI_divWithVar                (SLVM_state * state, std::string code[]) {
		addr_t addr = state->get_var(code[state->instruction_pointer + 1]);
		state->accumulator.set_num(state->accumulator.get_num() / state->memory[addr].get_num());
		state->instruction_pointer ++; // it will be incremented by the caller a second time
	}
	void fI_bitwiseLsfWithVar         (SLVM_state * state, std::string code[]) {
		addr_t addr = state->get_var(code[state->instruction_pointer + 1]);
		state->accumulator.set_num(addr_t(state->accumulator.get_num()) << addr_t(state->memory[addr].get_num()));
		state->instruction_pointer ++; // it will be incremented by the caller a second time
	}
	void fI_bitwiseRsfWithVar         (SLVM_state * state, std::string code[]) {
		addr_t addr = state->get_var(code[state->instruction_pointer + 1]);
		state->accumulator.set_num(addr_t(state->accumulator.get_num()) >> addr_t(state->memory[addr].get_num()));
		state->instruction_pointer ++; // it will be incremented by the caller a second time
	}
	void fI_bitwiseAndWithVar         (SLVM_state * state, std::string code[]) {
		addr_t addr = state->get_var(code[state->instruction_pointer + 1]);
		state->accumulator.set_num(addr_t(state->accumulator.get_num()) & addr_t(state->memory[addr].get_num()));
		state->instruction_pointer ++; // it will be incremented by the caller a second time
	}
	void fI_bitwiseOrWithVar          (SLVM_state * state, std::string code[]) {
		addr_t addr = state->get_var(code[state->instruction_pointer + 1]);
		state->accumulator.set_num(addr_t(state->accumulator.get_num()) | addr_t(state->memory[addr].get_num()));
		state->instruction_pointer ++; // it will be incremented by the caller a second time
	}
	void fI_modWithVar                (SLVM_state * state, std::string code[]) {
		addr_t addr = state->get_var(code[state->instruction_pointer + 1]);
		state->accumulator.set_num(addr_t(state->accumulator.get_num()) % addr_t(state->memory[addr].get_num()));
		state->instruction_pointer ++; // it will be incremented by the caller a second time
	}
	void fI_print                     (SLVM_state * state, std::string code[]) {
		printf("%s",state->accumulator.get_string().c_str());
	}
	void fI_println                   (SLVM_state * state, std::string code[]) {
		printf("%s\n",state->accumulator.get_string().c_str());
	}
	void fI_jmp                       (SLVM_state * state, std::string code[]) {
		addr_t addr = atoi(code[state->instruction_pointer + 1].c_str());
		state->instruction_pointer = addr;
		state->instruction_pointer++;
	}
	void fI_jt                        (SLVM_state * state, std::string code[]) {
		addr_t addr = atoi(code[state->instruction_pointer + 1].c_str());
		if (state->accumulator.get_num()){
			state->instruction_pointer = addr;
		}
		state->instruction_pointer++;
	}
	void fI_jt                        (SLVM_state * state, std::string code[]) {
		addr_t addr = atoi(code[state->instruction_pointer + 1].c_str());
		if (!state->accumulator.get_num()){
			state->instruction_pointer = addr;
		}
		state->instruction_pointer++;
	}
	void fI_boolAndWithVar            (SLVM_state * state, std::string code[]) {
		addr_t addr = state->get_var(code[state->instruction_pointer + 1]);
		state->accumulator.set_num(addr_t(state->accumulator.get_num()) && addr_t(state->memory[addr].get_num()));
		state->instruction_pointer ++; // it will be incremented by the caller a second time
	}
	void fI_boolOrWithVar             (SLVM_state * state, std::string code[]) {
		addr_t addr = state->get_var(code[state->instruction_pointer + 1]);
		state->accumulator.set_num(addr_t(state->accumulator.get_num()) || addr_t(state->memory[addr].get_num()));
		state->instruction_pointer ++; // it will be incremented by the caller a second time
	}
	void fI_boolEqualWithVar          (SLVM_state * state, std::string code[]) {
		addr_t addr = state->get_var(code[state->instruction_pointer + 1]);
		state->accumulator.set_num(addr_t(state->accumulator.get_num()) == addr_t(state->memory[addr].get_num()));
		state->instruction_pointer ++; // it will be incremented by the caller a second time
	}
	void fI_largerThanOrEqualWithVar  (SLVM_state * state, std::string code[]) {
		addr_t addr = state->get_var(code[state->instruction_pointer + 1]);
		state->accumulator.set_num(addr_t(state->accumulator.get_num()) >= addr_t(state->memory[addr].get_num()));
		state->instruction_pointer ++; // it will be incremented by the caller a second time
	}
	void fI_smallerThanOrEqualWithVar (SLVM_state * state, std::string code[]) {
		addr_t addr = state->get_var(code[state->instruction_pointer + 1]);
		state->accumulator.set_num(addr_t(state->accumulator.get_num()) <= addr_t(state->memory[addr].get_num()));
		state->instruction_pointer ++; // it will be incremented by the caller a second time
	}
	void fI_boolNotEqualWithVar       (SLVM_state * state, std::string code[]) {
		addr_t addr = state->get_var(code[state->instruction_pointer + 1]);
		state->accumulator.set_num(addr_t(state->accumulator.get_num()) != addr_t(state->memory[addr].get_num()));
		state->instruction_pointer ++; // it will be incremented by the caller a second time
	}
	void fI_smallerThanWithVar        (SLVM_state * state, std::string code[]) {
		addr_t addr = state->get_var(code[state->instruction_pointer + 1]);
		state->accumulator.set_num(addr_t(state->accumulator.get_num()) < addr_t(state->memory[addr].get_num()));
		state->instruction_pointer ++; // it will be incremented by the caller a second time
	}
	void fI_largerThanWithVar         (SLVM_state * state, std::string code[]) {
		addr_t addr = state->get_var(code[state->instruction_pointer + 1]);
		state->accumulator.set_num(addr_t(state->accumulator.get_num()) > addr_t(state->memory[addr].get_num()));
		state->instruction_pointer ++; // it will be incremented by the caller a second time
	}
	void fI_putPixel                  (SLVM_state * state, std::string code[]) {
		addr_t x = state->get_var(code[state->instruction_pointer + 1]);
		addr_t y = state->get_var(code[state->instruction_pointer + 2]);

		GraphicInstruction gi;
		gi.instruction = GI_P_PX;
		gi.data.D_GI_P_PX.x = state->memory[x].get_num();
		gi.data.D_GI_P_PX.y = state->memory[y].get_num();
		state->graphic_queue.push(gi);

		state->instruction_pointer += 2;
	}
	void fI_putLine                   (SLVM_state * state, std::string code[]) {
		addr_t x0 = state->get_var(code[state->instruction_pointer + 1]);
		addr_t y0 = state->get_var(code[state->instruction_pointer + 2]);
		addr_t x1 = state->get_var(code[state->instruction_pointer + 1]);
		addr_t y1 = state->get_var(code[state->instruction_pointer + 2]);

		GraphicInstruction gi;
		gi.instruction = GI_P_LN;
		gi.data.D_GI_P_LN.x0 = state->memory[x0].get_num();
		gi.data.D_GI_P_LN.y0 = state->memory[y0].get_num();
		gi.data.D_GI_P_LN.x1 = state->memory[x1].get_num();
		gi.data.D_GI_P_LN.y1 = state->memory[y1].get_num();
		state->graphic_queue.push(gi);

		state->instruction_pointer += 2;
	}
	void fI_putRect                   (SLVM_state * state, std::string code[]) {
		addr_t x = state->get_var(code[state->instruction_pointer + 1]);
		addr_t y = state->get_var(code[state->instruction_pointer + 2]);
		addr_t w = state->get_var(code[state->instruction_pointer + 1]);
		addr_t h = state->get_var(code[state->instruction_pointer + 2]);

		GraphicInstruction gi;
		gi.instruction = GI_P_REC;
		gi.data.D_GI_P_REC.x = state->memory[x].get_num();
		gi.data.D_GI_P_REC.y = state->memory[y].get_num();
		gi.data.D_GI_P_REC.w = state->memory[w].get_num();
		gi.data.D_GI_P_REC.h = state->memory[h].get_num();
		state->graphic_queue.push(gi);

		state->instruction_pointer += 2;
	}

	// thank you https://stackoverflow.com/a/5488718/12469275
	void (*func[])(SLVM_state *state, std::string *code) = {
		fI_unknown,
		fI_ldi,
		fI_loadAtVar,
		fI_storeAtVar,
		fI_jts,
		fI_ret,
		fI_addWithVar,
		fI_subWithVar,
		fI_mulWithVar,
		fI_divWithVar,
		fI_bitwiseLsfWithVar,
		fI_bitwiseRsfWithVar,
		fI_bitwiseAndWithVar,
		fI_bitwiseOrWithVar,
		fI_modWithVar,
		fI_print,
		fI_println,
		fI_jmp,
		fI_jt,
		fI_jt,
		fI_boolAndWithVar,
		fI_boolOrWithVar,
		fI_boolEqualWithVar,
		fI_largerThanOrEqualWithVar,
		fI_smallerThanOrEqualWithVar,
		fI_boolNotEqualWithVar,
		fI_smallerThanWithVar,
		fI_largerThanWithVar,
		fI_putPixel,
		fI_putLine,
		fI_putRect
	};
}

void SLVM_state::process(std::string code[], Instruction instructions[]) {
		switch (instructions[instruction_pointer]) {
			case I_unknown:
				printf("Unknown instruction at %d (`%s`)\n", instruction_pointer, code[instruction_pointer].c_str());
				running = false;
				break;
			default:
				if (instructions[instruction_pointer] > Instructions::last_impl) {
					printf("Unimplemented instruction at %d (`%s`)\n", instruction_pointer, code[instruction_pointer].c_str());
					printf("You can help by contributing to the project on GitHub!\n");
					printf("www.github.com/MinekPo1/CSLVM/contribute\n");
					running = false;
					break;
				}
				Instructions::func[instructions[instruction_pointer]](this, code);
				break;

		}
		instruction_pointer++;
	}
