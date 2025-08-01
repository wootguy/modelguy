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

	// textures affected by topcolor/bottomcolor
	bool hasRemappableTextures();

	mstudiobodyparts_t* get_body(int bodyIdx);

	mstudiomodel_t* get_model(int bodyIdx, int modelIdx);

	mstudiomesh_t* get_mesh(int bodyIdx, int modelIdx, int meshIdx);

	mstudiotexture_t* get_texture(int idx);

	mstudioseqdesc_t* get_sequence(int idx);

	// bytes for mesh triangle commands
	int getMeshTriSize(int bodyIdx, int modelIdx, int meshIdx);

	// merge other model into this one as a submodel
	bool addSubmodel(Model& otherModel);

	// move/rotate the model so that it displays correctly when the root bone has 0,0,0 rotation and position
	void transformForZeroedRootBone();

	// insertOffset = account for padding
	int getSubmodelAppendOffset(int bodypart, int model);

	// model has no triangles?
	bool isEmpty();

	bool mergeExternalTextures(bool deleteSource);

	void addTextures(Model& otherModel);

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
	// returns: 0 = fail, 1 = success, 2 = no porting needed
	int port_sc_textures_to_hl(int maxPixels);

	// converts a sven co-op model for use in half-life
	// noanim = don't adjust animation ordering
	// returns: 0 = fail, 1 = success, 2 = no porting needed
	int port_to_hl(bool forcePortFromSven=false, bool noanim=false);

	// figure out which mod this player model was made for
	int get_model_type(bool printResult=false);

	void printModelDataOrder(); // for debugging

	// deduplicates data
	void optimize();

	// get animation data as an array of shorts
	vector<short> get_animation_data(int sequence);

	// get size of animation data
	int get_animation_size(int sequence);

private:
	string fpath;

	// align = add alignment bytes if inserted data length is unaligned, and update indexes
	// returns bytes inserted
	int insertData(void * src, size_t bytes, bool alignAndUpdate=false);

	void removeData(size_t bytes);

	// updates all indexes with values greater than 'afterIdx', adding 'delta' to it.
	void updateIndexes(int afterIdx, int delta);
};