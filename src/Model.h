#pragma once

#include "types.h"
#include "studio.h"
#include <string>
#include <vector>
#include "mstream.h"
#include "ModelType.h"

class Model {
public:
	studiohdr_t* header;
	mstream data;

	Model(string fpath);
	~Model();

	bool validate();

	bool isExtModel();

	// has a modelT.mdl?
	bool hasExternalTextures();

	// has at least a model01.mdl?
	bool hasExternalSequences(); 

	// model has no triangles?
	bool isEmpty();

	bool mergeExternalTextures(bool deleteSource);

	bool mergeExternalSequences(bool deleteSource);

	bool cropTexture(string texName, int width, int height);

	bool resizeTexture(string texName, int newWidth, int newHeight);

	bool renameTexture(string cropName, string newName);

	void write(string fpath);

	// caller is responsible for deleting the ptr
	vector<vec3> getVertexes();

	// write model info to a json file
	void dump_info(string outputPath);

	// apply .wav extension to all model event sounds
	void wavify();

	// emulate flat shading by pointing all surface normals in the same direction
	bool hackshade();

	// emulate flatshade effect and resize textures that are too big
	bool port_to_hl();

	// figure out which mod this player model was made for
	int get_model_type();

private:
	string fpath;

	void insertData(void * src, size_t bytes);

	void removeData(size_t bytes);

	// updates all indexes with values greater than 'afterIdx', adding 'delta' to it.
	void updateIndexes(int afterIdx, int delta);
};