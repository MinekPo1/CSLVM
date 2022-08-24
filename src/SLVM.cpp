#include <GLFW/glfw3.h>
#include <map>
#include <stdio.h>
#include <string>
#include <stack>
#include <vector>
#include "pre-parser.cpp"
#include <queue>
#include <math.h>
#include <chrono>
#include <thread>

// this allows an easy hop to data types with more capacity
#define num_t float
#define addr_t int

// shortcut to get variable in front of the program pointer
#define get_var_with_offset(n) state->get_var(code[state->instruction_pointer + n])
// shortcuts to get value at address
#define m_get_num(addr) state->memory[addr].get_num()
#define m_get_str(addr) state->memory[addr].get_string()

addr_t MEMORY_SIZE = 0x10000;

struct MemoryCell {
	union {
		num_t n;
		std::string * s;
	} value;
	bool is_num;

	num_t get_num() {
		if (is_num)
			return value.n;
		// convert string to num_t
		// note: we can loose some accuracy
		//       since stof returns float
		return std::stof(*value.s);
	}

	std::string get_string() {
		if (is_num)
			return std::to_string(value.n);
		return *value.s;
	}

	void set_num(num_t n) {
		value.n = n;
		is_num = true;
	}

	void set_string(std::string s) {
		value.s = new std::string(s);
		is_num = false;
	}

	MemoryCell() {
		is_num = true;
		value.n = 0;
	}

	~MemoryCell() {
		if (!is_num)
			delete value.s;
	}
};

enum GraphicInstructions{
	GI_P_PX,
	GI_P_LN,
	GI_P_REC,
	GI_P_TXT,
	GI_GOTO,
	GI_S_CL
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

	void process(InstructionStorage store);

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
	Instruction last_impl = I_mouseY;
	// why are the function arguments r padded?
	// because no one stopped me.
	void fI_ldi                       (SLVM_state * state, std::string code[]) {
		state->accumulator.set_string(code[state->instruction_pointer + 1]);
		state->instruction_pointer ++; // it will be incremented by the caller a second time
	}
	void fI_loadAtVar                 (SLVM_state * state, std::string code[]) {
		addr_t addr = state->get_var(code[state->instruction_pointer + 1]);
		state->accumulator.value = state->memory[addr].value;
		state->accumulator.is_num = state->memory[addr].is_num;
		state->instruction_pointer ++; // it will be incremented by the caller a second time
	}
	void fI_storeAtVar                (SLVM_state * state, std::string code[]) {
		addr_t addr = state->get_var(code[state->instruction_pointer + 1]);
		state->memory[addr].value = state->accumulator.value;
		state->memory[addr].is_num = state->accumulator.is_num;
		state->instruction_pointer ++; // it will be incremented by the caller a second time
	}
	void fI_jts                       (SLVM_state * state, std::string code[]) {
		// jump to stack
		addr_t addr = atoi(code[state->instruction_pointer + 1].c_str());
		state->call_stack.push(state->instruction_pointer + 1);
		state->instruction_pointer = addr - 1;
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
		state->instruction_pointer = addr - 1;
	}
	void fI_jt                        (SLVM_state * state, std::string code[]) {
		addr_t addr = atoi(code[state->instruction_pointer + 1].c_str());
		if (state->accumulator.get_num() > 0){
			printf("yas queen\n");
			state->instruction_pointer = addr - 1;
		}
		else
			state->instruction_pointer++;
	}
	void fI_jf                        (SLVM_state * state, std::string code[]) {
		addr_t addr = atoi(code[state->instruction_pointer + 1].c_str());
		if (state->accumulator.get_num() < 1){
			printf("hahahaha\n");
			state->instruction_pointer = addr - 1;
		}
		else
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
	void fI_setColor                  (SLVM_state * state, std::string code[]) {
		addr_t c = get_var_with_offset(1);
		GraphicInstruction gi;
		gi.instruction = GI_S_CL;
		gi.data.D_GI_S_CL.cl = state->memory[c].get_num();
		state->graphic_queue.push(gi);
		state->instruction_pointer ++;
	}
	void fI_clg                       (SLVM_state * state, std::string code[]) {
		while (!state->graphic_queue.empty())
			state->graphic_queue.pop();
	}
	void fI_done                      (SLVM_state * state, std::string code[]) {
		state->running = false;
	}
	void fI_malloc                    (SLVM_state * state, std::string code[]) {
		addr_t size = get_var_with_offset(1);

		state->accumulator.set_num(
			state->allocate_memory(state->memory[size].get_num())
		);
	}
	void fI_round                     (SLVM_state * state, std::string code[]) {
		addr_t val = get_var_with_offset(1);
		addr_t places = get_var_with_offset(2);

		num_t pow10 = pow(10,state->memory[places].get_num());

		state->accumulator.set_num(
			round(m_get_num(val) * pow10) / pow10
		);
		state->instruction_pointer ++;
		state->instruction_pointer ++;
	}
	void fI_floor                     (SLVM_state * state, std::string code[]) {
		addr_t val = get_var_with_offset(1);
		addr_t places = get_var_with_offset(2);

		num_t pow10 = pow(10,state->memory[places].get_num());

		state->accumulator.set_num(
			floor(m_get_num(val) * pow10) / pow10
		);
		state->instruction_pointer ++;
		state->instruction_pointer ++;
	}
	void fI_ceil                      (SLVM_state * state, std::string code[]) {
		addr_t val = get_var_with_offset(1);
		addr_t places = get_var_with_offset(2);

		num_t pow10 = pow(10,state->memory[places].get_num());

		state->accumulator.set_num(
			ceil(m_get_num(val) * pow10) / pow10
		);
		state->instruction_pointer ++;
		state->instruction_pointer ++;
	}
	void fI_sin                       (SLVM_state * state, std::string code[]) {
		addr_t val = get_var_with_offset(1);

		state->accumulator.set_num(
			sin(m_get_num(val))
		);
		state->instruction_pointer ++;
	}
	void fI_cos                       (SLVM_state * state, std::string code[]) {
		addr_t val = get_var_with_offset(1);

		state->accumulator.set_num(
			sin(m_get_num(val))
		);
		state->instruction_pointer ++;
	}
	void fI_sqrt                      (SLVM_state * state, std::string code[]) {
		addr_t val = get_var_with_offset(1);

		state->accumulator.set_num(
			sqrt(m_get_num(val))
		);
		state->instruction_pointer ++;
	}
	void fI_atan2                     (SLVM_state * state, std::string code[]) {
		addr_t a = get_var_with_offset(1);
		addr_t b = get_var_with_offset(2);

		state->accumulator.set_num(
			atan2(m_get_num(a),m_get_num(b))
		);
		state->instruction_pointer ++;
		state->instruction_pointer ++;
	}
	// TODO: fI_mouseDown, fI_mouseX, fI_mouseY
	void fI_sleep                     (SLVM_state * state, std::string code[]) {
		addr_t time = get_var_with_offset(1);

		std::this_thread::sleep_for(std::chrono::milliseconds(m_get_num(time)));

		state->instruction_pointer ++;
	}
	void fI_drawText                  (SLVM_state * state, std::string code[]) {
		addr_t text = get_var_with_offset(1);

		GraphicInstruction gi;

		gi.instruction = GI_P_TXT;
		gi.data.D_GI_P_TXT.text = new std::string(m_get_str(text));

		state->graphic_queue(gi);
	}
	void fI_loadAtVarWithOffset       (SLVM_state * state, std::string code[]) {
		addr_t addr = state->get_var(code[state->instruction_pointer + 1]);
		addr_t offset = get_var_with_offset(2);
		addr += m_get_num(offset);
		state->accumulator.value = state->memory[addr].value;
		state->accumulator.is_num = state->memory[addr].is_num;
		state->instruction_pointer ++; // it will be incremented by the caller a second time
	}
	void fI_storeAtVarWithOffset      (SLVM_state * state, std::string code[]) {
		addr_t addr = state->get_var(code[state->instruction_pointer + 1]);
		addr_t offset = get_var_with_offset(2);
		addr += m_get_num(offset);
		state->memory[addr].value = state->accumulator.value;
		state->memory[addr].is_num = state->accumulator.is_num;
		state->instruction_pointer ++; // it will be incremented by the caller a second time
	}
	// TODO: fI_isKeyPressed
	void fI_createColor               (SLVM_state * state, std::string code[]) {
		addr_t r = get_var_with_offset(1);
		addr_t g = get_var_with_offset(2);
		addr_t b = get_var_with_offset(3);
		// do ***math***
		state->accumulator.set_num(
			int(m_get_num(r)) << 16 +
			int(m_get_num(g)) << 8  +
			int(m_get_num(b))
		);
		state->instruction_pointer ++;
		state->instruction_pointer ++;
		state->instruction_pointer ++;
	}
	void fI_charAt                    (SLVM_state * state, std::string code[]) {
		addr_t text = get_var_with_offset(1);
		addr_t index = get_var_with_offset(2);

		state->accumulator.set_string(
			{m_get_str(text)[m_get_num(index)]}
		);
		state->instruction_pointer ++;
		state->instruction_pointer ++;
	}
	void fI_sizeOf                    (SLVM_state * state, std::string code[]) {
		addr_t text = get_var_with_offset(1);

		state->accumulator.set_num(
			m_get_str(text).length()
		);
		state->instruction_pointer ++;
	}
	void fI_contains                  (SLVM_state * state, std::string code[]) {
		addr_t text = get_var_with_offset(1);
		addr_t sub_text = get_var_with_offset(2);

		state->accumulator.set_num(
			m_get_str(text).find(sub_text)
		);
		state->instruction_pointer ++;
		state->instruction_pointer ++;
	}

	void fI_TODO                      (SLVM_state * state, std::string code[]) {
		printf(
			"Unimplemented instruction %s @ %i\n",
			code[state->instruction_pointer].c_str(),
			state->instruction_pointer + 1
		);
		printf("You can help by contributing!\n");
		state->running = false;
	}

	// thank you https://stackoverflow.com/a/5488718/12469275
	void (*func[])(SLVM_state *state, std::string *code) = {
		NULL,
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
		fI_jf,
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
		fI_putRect,
		fI_setColor,
		fI_clg,
		fI_done,
		fI_malloc,
		fI_round,
		fI_floor,
		fI_ceil,
		fI_sin,
		fI_cos,
		fI_sqrt,
		fI_atan2,
		fI_TODO, // I_mouseDown
		fI_TODO, // I_mouseX
		fI_TODO, // I_mouseY
	};
}

void SLVM_state::process(InstructionStorage store) {
		if (instruction_pointer >= store.size){
			this->running = false;
			return;
		}
		Instruction i = store.get_at(this->instruction_pointer);
		if (!i){
			printf(
				"Unknown instruction %s @ %i\n",
				store.values[this->instruction_pointer].c_str(),
				this->instruction_pointer + 1
			);
			this->running = false;
			return;
		}
		if (i > Instructions::last_impl){
			printf(
				"Unimplemented instruction %s @ %i (#%d)\n",
				store.values[this->instruction_pointer].c_str(),
				this->instruction_pointer + 1,
				i
			);
			printf("You can help by contributing!\n");
			this->running = false;
			return;
		}
		Instructions::func[i](this,store.values);
		instruction_pointer++;
	}
