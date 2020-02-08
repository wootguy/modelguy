#include "util.h"
#include "studio.h"
#include "Model.h"
#include <string>
#include <algorithm>
#include <iostream>

int merge_model(string inputFile, string outputFile) {
	Model model(inputFile);

	if (!model.validate())
		return 1;

	if (!model.hasExternalTextures() && !model.hasExternalSequences()) {
		cout << "Model has no external textures or sequences!\n";
		return 1;
	}

	bool deleteSources = inputFile == outputFile;
	model.mergeExternalSequences(deleteSources);
	model.mergeExternalTextures(deleteSources);

	model.write(outputFile);
	
	return 0;
}

int crop_texture(string inputFile, string outputFile, string texName, int width, int height) {
	Model model(inputFile);

	if (!model.validate())
		return 1;

	if (!model.cropTexture(texName, width, height))
		return 1;

	model.write(outputFile);

	return 0;
}

int main(int argc, char* argv[])
{
	// parse command-line args
	string inputFile;
	string outputFile;
	string command;
	string texName;
	int cropWidth = 0;
	int cropHeight = 0;

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

			if (command == "crop") {
				if (i == 2) {
					texName = arg;
				}
				else if (i == 3) {
					int xidx = larg.find_first_of("x");
					cropWidth = atoi(larg.substr(0, xidx).c_str());
					cropHeight = atoi(larg.substr(xidx+1).c_str());
				}
			}
		}

		if (larg.find("-help") == 0 || argc <= 1)
		{
			cout <<
			"This tool modifies Half-Life models without decompiling them.\n\n"
			"Usage: modelguy <command> <parameters> <input.mdl> [output.mdl]\n"

			"\n<Commands>\n"
			"  merge : Merges external texture and sequence models into the output model.\n"
			"          If no output file is specified, the external models will also be deleted.\n"
			"  crop  : Crops a texture to the specified dimensions. Used after compiling model.\n"
			"          Takes <width>x<height> as parameters.\n\n"

			"\nExamples:\n"
			"  modelguy merge barney.mdl\n"
			"  modelguy crop face.bmp 100x80 hgrunt.mdl\n"
			;
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
	else if (command == "crop") {
		if (texName.size() == 0) {
			cout << "ERROR: No texture name specified\n";
			return 1;
		}
		if (cropWidth <= 0 || cropHeight <= 0) {
			cout << "ERROR: Bad crop dimentions: " << cropWidth << "x" << cropHeight << endl;
			return 1;
		}
		return crop_texture(inputFile, outputFile, texName, cropWidth, cropHeight);
	}
	else {
		cout << "unrecognized command: " << command << endl;
	}

	return 0;
}

