#include "util.h"
#include "studio.h"
#include "Model.h"
#include "Renderer.h"
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

int merge_prop(string mainModel, string subModel, string outputmodel) {
	Model model(mainModel);

	if (!model.validate())
		return 1;

	Model submodel(subModel);
	if (!submodel.validate())
		return 1;

	if (model.hasExternalTextures()) {
		model.mergeExternalSequences(true);
	}
	if (submodel.hasExternalTextures()) {
		model.mergeExternalTextures(false);
	}

	if (!model.addSubmodel(submodel)) {
		return 1;
	}

	model.write(outputmodel);

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

int resize_texture(string inputFile, string outputFile, string texName, int width, int height) {
	Model model(inputFile);

	if (!model.validate())
		return 1;

	if (!model.resizeTexture(texName, width, height))
		return 1;

	model.write(outputFile);

	return 0;
}


int rename_texture(string inputFile, string outputFile, string texName, string newTexName) {
	Model model(inputFile);

	if (!model.validate())
		return 1;

	if (!model.renameTexture(texName, newTexName))
		return 1;

	model.write(outputFile);

	return 0;
}

void dump_info(string inputFile, string outputFile) {
	Model model(inputFile);
	model.validate();
	model.dump_info(outputFile);
}

void wavify(string inputFile, string outputFile) {
	Model model(inputFile);
	model.validate();

	int eventsEdited = model.wavify();

	if (eventsEdited) {
		printf("Applied wav extension to %d audio events\n", eventsEdited);
	}

	model.write(outputFile);
}

int port_hl(string inputFile, string outputFile, bool force, bool noanim) {
	Model model(inputFile);
	model.validate();

	int ret = model.port_to_hl(force, noanim);

	if (ret == 2) { // no port needed
		return 2;
	}
	if (ret == 0) { // fail
		return 1;
	}
	
	model.write(outputFile);
	return 0;
}

void optimize_model(string inputFile, string outputFile) {
	Model model(inputFile);
	model.validate();
	model.optimize();
	model.write(outputFile);
}

int get_model_type(string inputFile) {
	Model model(inputFile);
	return model.get_model_type(true);
}

void data_layout_model(string inputFile) {
	Model model(inputFile);
	model.printModelDataOrder();
}

int view_model(string inputFile) {
	bool legacy = false;
	bool headless = false;
	Renderer renderer = Renderer(inputFile, 500, 800, legacy, headless);
	renderer.render_loop();
	return 0;
}

int image_model(string inputFile, string outputFile, int width, int height) {
	bool legacy = true;
	bool headless = true;
	Renderer renderer = Renderer(inputFile, width, height, legacy, headless);
	renderer.create_image(outputFile);
	return 0;
}

#ifndef EMSCRIPTEN

int main(int argc, char* argv[])
{
	// parse command-line args
	string inputFile;
	string outputFile;
	string command;
	string texName;
	string newTexName;
	string submodelFile;
	int cropWidth = 0;
	int cropHeight = 0;
	bool force = false;
	bool noanim = false;

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
				else if (command == "mergeprop" && submodelFile.empty())
					submodelFile = arg;
				else
					outputFile = arg;
			}

			if (command == "crop" || command == "resize" || command == "image") {
				if (i == 2) {
					texName = arg;
				}
				else if (i == 3) {
					int xidx = larg.find_first_of("x");
					cropWidth = atoi(larg.substr(0, xidx).c_str());
					cropHeight = atoi(larg.substr(xidx+1).c_str());
				}
			}

			if (command == "image") {
				if (i == 4) {
					outputFile = arg;
				}
			}
			if (command == "rename") {
				if (i == 2) {
					texName = arg;
				}
				else if (i == 3) {
					newTexName = arg;
				}
			}
			if (command == "info" || command == "layout" || command == "wavify" || command == "optimize") {
				if (i == 2) {
					inputFile = arg;
				}
				else if (i == 3) {
					outputFile = arg;
				}
			}
			if (command == "porthl") {
				if (arg == "-f") {
					force = true;
				}
				if (arg == "-noanim") {
					noanim = true;
				}
			}
		}

		if (larg.find("-help") == 0 || argc <= 1)
		{
			cout <<
			"This tool modifies Half-Life models without decompiling them.\n\n"
			"Usage: modelguy <command> <parameters> <input.mdl> [output.mdl]\n"

			"\n<Commands>\n"
			"  merge     : Merges external texture and sequence models into the output model.\n"
			"              If no output file is specified, the external models will also be deleted.\n"
			"  mergeprop : another one model into another as a body part. Used for prop models.\n"
			"  crop      : Crops a texture to the specified dimensions. Takes <width>x<height> as parameters.\n"
			"  resize    : Resizes a texture to the specified dimensions. Takes <width>x<height> as parameters.\n"
			"  rename    : Renames a texture. Takes <old name> <new name> as parameters.\n"
			"  info      : Write model info to a JSON file. Takes <input.mdl> <output.json> as parameters.\n"
			"  wavify    : Apply .wav extension to all events. Takes <input.mdl> <output.json> as parameters\.n\n"
			"  porthl    : Port a Sven Co-op player model to Half-Life. Takes <input.mdl> and <output.mdl> as parameters.\n"
			"  type      : Identify player model type. The return code is unique per mod.\n"
			"  view      : View the model in 3D.\n"
			"  image     : Saves a PNG image of the model. Takes <width>x<height> and <output.png> as parameters.\n"
			"  layout    : Show data layout for the MDL file.\n"
			"  optimize  : deduplicate data.\n\n"

			"\nExamples:\n"
			"  modelguy merge barney.mdl\n"
			"  modelguy crop face.bmp 100x80 hgrunt.mdl\n"
			"  modelguy rename hev_arm.bmp Remap1_000_255_255.bmp v_shotgun.mdl\n"
			"  modelguy image player.mdl 800x400 player.png\n"
			;
			return 0;
		}
	}
	
	if (inputFile.size() == 0)
	{
		cout << "ERROR: No input file specified.\n";
		return 1;
	}
	if (outputFile.size() == 0) {
		if (command == "info") {
			outputFile = replaceString(inputFile, ".mdl", ".json");
		}
		else {
			outputFile = inputFile;
		}
	}

	if (!fileExists(inputFile)) {
		cout << "ERROR: File does not exist: " << inputFile << endl;
		return 1;
	}

	if (command == "merge") {
		return merge_model(inputFile, outputFile);
	}
	if (command == "mergeprop") {
		return merge_prop(inputFile, submodelFile, outputFile);
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
	else if (command == "resize") {
		if (texName.size() == 0) {
			cout << "ERROR: No texture name specified\n";
			return 1;
		}
		if (cropWidth <= 0 || cropHeight <= 0) {
			cout << "ERROR: Bad resize dimentions: " << cropWidth << "x" << cropHeight << endl;
			return 1;
		}
		return resize_texture(inputFile, outputFile, texName, cropWidth, cropHeight);
	}
	else if (command == "rename") {
		if (texName.size() == 0) {
			cout << "ERROR: No texture name specified\n";
			return 1;
		}
		if (newTexName.size() == 0) {
			cout << "ERROR: No new texture name specified\n";
			return 1;
		}
		if (newTexName.size() > 64) {
			cout << "ERROR: New texture name must be 64 characters or fewer\n";
			return 1;
		}
		return rename_texture(inputFile, outputFile, texName, newTexName);
	}
	else if (command == "info") {
		if (inputFile.size() == 0) {
			cout << "ERROR: No input file specified\n";
			return 1;
		}
		if (outputFile.size() == 0) {
			cout << "ERROR: No output file specified\n";
			return 1;
		}
		dump_info(inputFile, outputFile);
	}
	else if (command == "wavify") {
		if (inputFile.size() == 0) {
			cout << "ERROR: No input file specified\n";
			return 1;
		}
		if (outputFile.size() == 0) {
			cout << "ERROR: No output file specified\n";
			return 1;
		}
		wavify(inputFile, outputFile);
	}
	else if (command == "porthl") {
		if (inputFile.size() == 0) {
			cout << "ERROR: No input file specified\n";
			return 1;
		}
		if (outputFile.size() == 0) {
			cout << "ERROR: No output file specified\n";
			return 1;
		}
		return port_hl(inputFile, outputFile, force, noanim);
	}
	else if (command == "optimize") {
		if (inputFile.size() == 0) {
			cout << "ERROR: No input file specified\n";
			return 1;
		}
		if (outputFile.size() == 0) {
			cout << "ERROR: No output file specified\n";
			return 1;
		}
		optimize_model(inputFile, outputFile);
	}
	else if (command == "type") {
		if (inputFile.size() == 0) {
			cout << "ERROR: No input file specified\n";
			return 1;
		}
		return get_model_type(inputFile);
	}
	else if (command == "view") {
		if (inputFile.size() == 0) {
			cout << "ERROR: No input file specified\n";
			return 1;
		}
		return view_model(inputFile);
	}
	else if (command == "layout") {
		if (inputFile.size() == 0) {
			cout << "ERROR: No input file specified\n";
			return 1;
		}
		data_layout_model(inputFile);
	}
	else if (command == "image") {
		if (inputFile.size() == 0) {
			cout << "ERROR: No input file specified\n";
			return 1;
		}
		if (outputFile.size() == 0 || outputFile == inputFile) {
			outputFile = replaceString(inputFile, ".mdl", ".png");
		}
		return image_model(inputFile, outputFile, cropWidth, cropHeight);
	}
	else {
		cout << "unrecognized command: " << command << endl;
	}

	return 0;
}

#endif