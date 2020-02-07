#include "util.h"
#include "studio.h"
#include "Model.h"
#include <string>
#include <algorithm>
#include <iostream>

int merge_model(string inputFile, string outputFile) {
	cout << "Merging " << inputFile << endl;

	Model model(inputFile);

	if (!model.validate())
		return 1;

	if (!model.hasExternalTextures() && !model.hasExternalSequences()) {
		cout << "Model has no external textures or sequences!\n";
		return 1;
	}

	model.mergeExternalTextures();
	model.write(outputFile);
}

int main(int argc, char* argv[])
{
	// parse command-line args
	string inputFile;
	string outputFile;
	string command;

	bool expectPaletteFile = false;
	for (int i = 0; i < argc; i++)
	{
		string arg = argv[i];
		string larg = arg; // lowercase arg
		std::transform(larg.begin(), larg.end(), larg.begin(), ::tolower);

		if (i == 1) {
			command = larg;
		}
		if (i > 1)
		{
			size_t eq = larg.find("=");
			if (larg.find(".mdl") != string::npos) {
				if (inputFile.size() == 0)
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

	if (!fileExists(inputFile)) {
		cout << "ERROR: File does not exist: " << inputFile << endl;
		return 1;
	}

	if (command == "merge") {
		return merge_model(inputFile, outputFile);
	}

	return 0;
}

