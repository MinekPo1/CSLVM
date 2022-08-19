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

	SLVM_state() {
		memory = new MemoryCell[MEMORY_SIZE];
		instruction_pointer = 0;
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

	void process(std::string code[], Instruction instructions[]){
		switch (instructions[instruction_pointer]) {
			case I_unknown:
				printf("Unknown instruction at %d (`%s`)\n", instruction_pointer, code[instruction_pointer].c_str());
				break;

			default:
				printf("Unimplemented instruction at %d (`%s`)\n", instruction_pointer, code[instruction_pointer].c_str());
				printf("You can help by contributing to the project on GitHub!\n");
				printf("www.github.com/MinekPo1/CSLVM/contribute\n");
				break;
		}
	}
};
