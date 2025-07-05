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

	vector<vec3> getVertexes();

	// write model info to a json file
	void dump_info(string outputPath);

	// apply .wav extension to all model event sounds. Returns number of events edited.
	int wavify();

	// get start of animation data for sequence
	mstudioanim_t* getAnimFrames(int sequence);

	// extends the animation by duplicating the last frame
	bool padAnimation(int sequence, int newFrameCount);

	// reorder to work with HL and extend/shorten anims that play too fast/slow.
	bool port_sc_animations_to_hl();

	// resize textures to the given max pixel count
	void downscale_textures(int maxPixels);

	// converts a sven co-op model for use in half-life
	bool port_to_hl();

	// figure out which mod this player model was made for
	int get_model_type();

	void printModelDataOrder(); // for debugging

	// deduplicates data
	void optimize();

	// get animation data as an array of shorts
	vector<short> get_animation_data(int sequence);

	// get size of animation data
	int get_animation_size(int sequence);

private:
	string fpath;

	void insertData(void * src, size_t bytes);

	void removeData(size_t bytes);

	// updates all indexes with values greater than 'afterIdx', adding 'delta' to it.
	void updateIndexes(int afterIdx, int delta);
};