#include <GLFW/glfw3.h>
#include <map>
#include <stdio.h>
#include <string>
#include <stack>
#include <vector>
#include "pre-parser.cpp"

int MEMORY_SIZE = 0x10000;

struct MemoryCell {
	union {
		float f;
		std::string * s;
	} value;
	bool is_float;

	float get_float() {
		if (is_float)
			return value.f;
		// convert string to float
		return std::stof(*value.s);
	}

	std::string get_string() {
		if (is_float)
			return std::to_string(value.f);
		return *value.s;
	}

	void set_float(float f) {
		value.f = f;
		is_float = true;
	}

	void set_string(std::string s) {
		value.s = new std::string(s);
		is_float = false;
	}

	MemoryCell() {
		is_float = true;
		value.f = 0.0f;
	}

	~MemoryCell() {
		if (!is_float)
			delete value.s;
	}
};

struct SLVM_state{
	                      MemoryCell* memory;
	                      MemoryCell  accumulator;
	                             int  instruction_pointer;
	                 std::stack<int>  call_stack;
	      std::map<std::string, int>  lookup_table;
	std::vector<std::pair<int, int>>  free_chunks; // <start, length>
	                            bool  running;

	SLVM_state() {
		memory = new MemoryCell[MEMORY_SIZE];
		free_chunks.push_back(std::make_pair(0, MEMORY_SIZE));
		instruction_pointer = 0;
		running = true;
	}

	~SLVM_state() {
		delete[] memory;
	}

	int allocate_memory(int size) {
		auto it = free_chunks.begin();
		while (it != free_chunks.end()) {
			if (it->second == size) {
				instruction_pointer = it->first;
				int addr = it->first;
				free_chunks.erase(it);
				return addr;
			}
			if (it->second > size) {
				int addr = it->first;
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

	void deallocate_memory(int addr, int size) {
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

		std::pair<int, int> p = { addr, size };
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

	int get_var(std::string name) {
		if (lookup_table.find(name) == lookup_table.end()) {
			// create new variable
			int addr = allocate_memory(1);
			lookup_table[name] = addr;
		}
		return lookup_table[name];
	}
};

namespace Instructions {
	Instruction last_impl = I_println;
	void fI_unknown(SLVM_state * state, std::string code[]) {
		// this will never be called
	}
	void fI_ldi(SLVM_state * state, std::string code[]) {
		state->accumulator.set_string(code[state->instruction_pointer + 1]);
		state->instruction_pointer ++; // it will be incremented by the caller a second time
	}
	void fI_loadAtVar(SLVM_state * state, std::string code[]) {
		int addr = state->get_var(code[state->instruction_pointer + 1]);
		state->accumulator = state->memory[addr];
		state->instruction_pointer ++; // it will be incremented by the caller a second time
	}
	void fI_storeAtVar(SLVM_state * state, std::string code[]) {
		int addr = state->get_var(code[state->instruction_pointer + 1]);
		state->memory[addr] = state->accumulator;
		state->instruction_pointer ++; // it will be incremented by the caller a second time
	}
	void fI_jts(SLVM_state * state, std::string code[]) {
		// jump to stack
		int addr = atoi(code[state->instruction_pointer + 1].c_str());
		state->call_stack.push(state->instruction_pointer);
		state->instruction_pointer = addr;
		state->instruction_pointer ++;
	}
	void fI_ret(SLVM_state * state, std::string code[]) {
		// return from stack
		state->instruction_pointer = state->call_stack.top();
		state->call_stack.pop();
	}
	void fI_addWithVar(SLVM_state * state, std::string code[]) {
		int addr = state->get_var(code[state->instruction_pointer + 1]);
		state->accumulator.set_float(state->accumulator.get_float() + state->memory[addr].get_float());
		state->instruction_pointer ++; // it will be incremented by the caller a second time
	}
	void fI_subWithVar(SLVM_state * state, std::string code[]) {
		int addr = state->get_var(code[state->instruction_pointer + 1]);
		state->accumulator.set_float(state->accumulator.get_float() - state->memory[addr].get_float());
		state->instruction_pointer ++; // it will be incremented by the caller a second time
	}
	void fI_mulWithVar(SLVM_state * state, std::string code[]) {
		int addr = state->get_var(code[state->instruction_pointer + 1]);
		state->accumulator.set_float(state->accumulator.get_float() * state->memory[addr].get_float());
		state->instruction_pointer ++; // it will be incremented by the caller a second time
	}
	void fI_divWithVar(SLVM_state * state, std::string code[]) {
		int addr = state->get_var(code[state->instruction_pointer + 1]);
		state->accumulator.set_float(state->accumulator.get_float() / state->memory[addr].get_float());
		state->instruction_pointer ++; // it will be incremented by the caller a second time
	}
	void fI_bitwiseLsfWithVar(SLVM_state * state, std::string code[]) {
		int addr = state->get_var(code[state->instruction_pointer + 1]);
		state->accumulator.set_float(int(state->accumulator.get_float()) << int(state->memory[addr].get_float()));
		state->instruction_pointer ++; // it will be incremented by the caller a second time
	}
	void fI_bitwiseRsfWithVar(SLVM_state * state, std::string code[]) {
		int addr = state->get_var(code[state->instruction_pointer + 1]);
		state->accumulator.set_float(int(state->accumulator.get_float()) >> int(state->memory[addr].get_float()));
		state->instruction_pointer ++; // it will be incremented by the caller a second time
	}
	void fI_bitwiseAndWithVar(SLVM_state * state, std::string code[]) {
		int addr = state->get_var(code[state->instruction_pointer + 1]);
		state->accumulator.set_float(int(state->accumulator.get_float()) & int(state->memory[addr].get_float()));
		state->instruction_pointer ++; // it will be incremented by the caller a second time
	}
	void fI_bitwiseOrWithVar(SLVM_state * state, std::string code[]) {
		int addr = state->get_var(code[state->instruction_pointer + 1]);
		state->accumulator.set_float(int(state->accumulator.get_float()) | int(state->memory[addr].get_float()));
		state->instruction_pointer ++; // it will be incremented by the caller a second time
	}
	void fI_modWithVar(SLVM_state * state, std::string code[]) {
		int addr = state->get_var(code[state->instruction_pointer + 1]);
		state->accumulator.set_float(int(state->accumulator.get_float()) % int(state->memory[addr].get_float()));
		state->instruction_pointer ++; // it will be incremented by the caller a second time
	}
	void fI_print(SLVM_state * state, std::string code[]) {
		printf("%s",state->accumulator.get_string().c_str());
	}
	void fI_println(SLVM_state * state, std::string code[]) {
		printf("%s\n",state->accumulator.get_string().c_str());
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
