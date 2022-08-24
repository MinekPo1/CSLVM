#include <string>
#include <map>
#include <stdio.h>
#include <cstring>
#include <fstream>
#include <vector>

#include "pre-parser.cpp"
#include "SLVM.cpp"

struct Options{
	std::string input = "out.slvm.txt";
	bool graphics = false;
	bool dump = false;

	std::map<std::string, bool *> flags = {
		{"g", &graphics},
		{"--graphics", &graphics},
		{"d", &dump},
		{"--dump", &dump}
	};

	std::map<std::string, std::string *> arguments = {
		{"i", &input},
		{"--input", &input}
	};

	std::map<std::string, int *> multi_flags = {};
};

Options parse_arguments(int argc, char * argv[]){
	Options options;
	for (int i = 1; i < argc; i++) {
		std::string arg = argv[i];
		if (arg.length() > 1 && arg[0] == '-') {
			if (arg[1] == '-') {
				std::string flag = arg.substr(2);
				if (options.arguments.find(arg) != options.arguments.end()) {
					if (argc != i + 1){
						*options.arguments[arg] = argv[i + 1];
						i++;
					}
					else {
						printf("Value for %s not provided", arg.c_str());
					}
				} else if (options.flags.find(arg) != options.flags.end()) {
					*options.flags[arg] = true;
				} else if (options.multi_flags.find(arg) != options.multi_flags.end()) {
					*options.multi_flags[arg]++;
				} else {
					printf("Unknown flag: `%s`\n", flag.c_str());
				}
			} else {
				for (int j = 1; j < arg.length(); j++) {
					std::string flag = std::string({arg[j]});
					if (options.arguments.find(flag) != options.arguments.end()) {
						if (argc != i + 1){
							*options.arguments[flag] = argv[i + 1];
							i++;
						}
					} else if (options.flags.find(flag) != options.flags.end()) {
						*options.flags[flag] = true;
					} else if (options.multi_flags.find(flag) != options.multi_flags.end()) {
						*options.multi_flags[flag]++;
					} else {
						printf("Unknown flag: `-%s`\n", flag.c_str());
					}
				}
			}
		} else {
			options.input = arg;
		}
	}
	return options;
}

int main(int argc, char * argv[]){
	Options options = parse_arguments(argc, argv);
	// open file
	std::ifstream file(options.input.c_str());
	if (!file) {
		printf("Could not open file: %s\n", options.input.c_str());
		return 1;
	}
	// read file
	std::string line;
	size_t len = 0;
	std::vector<std::string> lines;
	while (std::getline(file, line)) {
		lines.push_back(line);
	}
	// close file
	file.close();
	// convert to an array
	std::string lines_array[lines.size()];
	for (int i = 0; i < lines.size(); i++) {
		lines_array[i] = lines[i];
	}

	InstructionStorage store(lines_array,lines.size());

	// execute
	SLVM_state state;
	while (state.running)
	{
		printf("[%s @ %d]\n",store.values[state.instruction_pointer].c_str(),state.instruction_pointer + 1);
		state.process(store);
	}


	return 0;
}
