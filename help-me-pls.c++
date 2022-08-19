#include <map>
#include <stdio.h>
#include <cstring>

struct cmp_str
{
	bool operator()(char const *a, char const *b) const
	{
		return std::strcmp(a, b) < 0;
	}
};

std::map<char *, int, cmp_str> map = {
	{"lexer", 42}
};

int main(int argc, char * argv[]){
	printf("%d", map[argv[1]]);
	return 0;
}
