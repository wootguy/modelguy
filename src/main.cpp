#include "studio.h"
#include <string>
#include <algorithm>
#include <iostream>

using namespace std;

int main(int argc, char* argv[])
{
	// parse command-line args
	string inputFile;
	string outputFile;

	bool expectPaletteFile = false;
	for (int i = 0; i < argc; i++)
	{
		string arg = argv[i];
		string larg = arg; // lowercase arg
		std::transform(larg.begin(), larg.end(), larg.begin(), ::tolower);

		if (i > 0)
		{
			size_t eq = larg.find("=");
			if (larg.find(".mdl") != string::npos) {
				if (inputFile.size() != 0)
					inputFile = arg;
				else
					outputFile = arg;
			}
		}

		if (larg.find("-help") == 0 || argc <= 1)
		{
			cout <<
			"This tool modifies Half-Life models without decompiling them.\n\n"
			"Usage: modelguy <command> <input.mdl> [output.mdl]\n"

			"\n\<Commands>\n"
			"  merge : Merges external texture and sequence models into the output model.\n\n"
			;
			system("pause");
			return 0;
		}
	}
	
	if (inputFile.size() == 0)
	{
		cout << "ERROR: No input file specified.\n";
		return 1;
	}
	if (outputFile.size() == 0)
		outputFile = inputFile;

	return 0;
}

