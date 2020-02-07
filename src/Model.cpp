#include "Model.h"
#include "util.h"
#include <iostream>
#include <fstream>

// from HLSDK
#define ALIGN( a ) a = (byte *)((int)((byte *)a + 3) & ~ 3)

Model::Model(string fpath)
{
	this->fpath = fpath;
	int len;
	char * buffer = loadFile(fpath, len);
	data = mstream(buffer, len);
	header = (studiohdr_t*)buffer;
}

Model::~Model()
{
	delete[] data.getBuffer();
}

bool Model::validate() {
	if (string(header->name).length() <= 0) {
		return false;
	}

	if (header->id != 1414743113) {
		cout << "ERROR: Invalid ID in model header\n";
		return false;
	}

	if (header->version != 10) {
		cout << "ERROR: Invalid version in model header\n";
		return false;
	}

	if (header->numseqgroups >= 10000) {
		cout << "ERROR: Too many seqgroups (" + to_string(header->numseqgroups) + ") in model\n";
		return false;
	}

	// Try loading required model info
	data.seek(header->bodypartindex);
	mstudiobodyparts_t* bod = (mstudiobodyparts_t*)data.get();

	if (data.eom())
		return false;

	for (int i = 0; i < bod->nummodels; i++) {
		data.seek(bod->modelindex + i * sizeof(mstudiomodel_t));
		mstudiomodel_t* mod = (mstudiomodel_t*)data.get();

		if (data.eom()) {
			cout << "ERROR: Failed to load body " + to_string(i) + "/" + to_string(bod->nummodels) + "\n";
			return false;
		}
	}

	return true;
}

bool Model::isEmpty() {
	bool isEmptyModel = true; // textures aren't needed if the model has no triangles

	data.seek(header->bodypartindex);
	mstudiobodyparts_t* bod = (mstudiobodyparts_t*)data.get();
	for (int i = 0; i < bod->nummodels; i++) {
		data.seek(bod->modelindex + i * sizeof(mstudiomodel_t));
		mstudiomodel_t* mod = (mstudiomodel_t*)data.get();

		if (mod->nummesh != 0) {
			isEmptyModel = false;
			break;
		}
	}

	return isEmptyModel;
}

bool Model::hasExternalTextures() {
	return header->numtextures == 0 && !isEmpty();
}

bool Model::hasExternalSequences() {
	return header->numseqgroups > 1;
}

char* Model::getTextures() {
	if (hasExternalTextures())
		return NULL;

	cout << "ZOMG DAT TEXTURES\n";
}

void Model::insertData(void* src, size_t bytes) {
	data.insert(src, bytes);
	header = (studiohdr_t*)data.getBuffer();
}

bool Model::mergeExternalTextures() {
	if (!hasExternalTextures()) {
		cout << "No external textures to merge\n";
		return false;
	}

	int lastDot = fpath.find_last_of(".");
	string ext = fpath.substr(lastDot);
	string basepath = fpath.substr(0, lastDot);
	string tpath = basepath + "t" + ext;

	if (!fileExists(tpath)) {
		tpath = basepath + "T" + ext;
		if (!fileExists(tpath)) {
			cout << "External texture model not found: " << tpath << endl;
			return false;
		}
	}

	Model tmodel(tpath);

	// copy info
	header->numtextures = tmodel.header->numtextures;
	header->numskinref = tmodel.header->numskinref;
	header->numskinfamilies = tmodel.header->numskinfamilies;
	

	// recalculate indexes
	data.seek(0, SEEK_END);
	size_t actualtextureindex = data.tell();
	size_t tmodel_textureindex = tmodel.header->textureindex;
	size_t tmodel_skinindex = tmodel.header->skinindex;
	size_t tmodel_texturedataindex = tmodel.header->texturedataindex;
	header->textureindex = actualtextureindex;
	header->skinindex = actualtextureindex + (tmodel_skinindex - tmodel_textureindex);
	header->texturedataindex = actualtextureindex + (tmodel_texturedataindex - tmodel_textureindex);

	// texture data is at the end of the file, with the structures grouped together
	tmodel.data.seek(tmodel_textureindex);
	insertData(tmodel.data.get(), tmodel.data.size() - tmodel.data.tell());

	// TODO: Align pointers or else model will crash or something? My test model has everything aligned.
	uint aligned = ((uint)header->texturedataindex + 3) & ~3;
	if (header->texturedataindex != aligned) {
		cout << "dataindex not aligned " << header->texturedataindex << endl;
	}
	aligned = ((uint)header->textureindex + 3) & ~3;
	if (header->textureindex != aligned) {
		cout << "texindex not aligned " << header->textureindex << endl;
	}
	aligned = ((uint)header->skinindex + 3) & ~3;
	if (header->skinindex != aligned) {
		cout << "skinindex not aligned " << header->skinindex << endl;
	}

	// recalculate indexes in the texture infos
	for (int i = 0; i < header->numtextures; i++) {
		data.seek(header->textureindex + i * sizeof(mstudiotexture_t));
		mstudiotexture_t* texture = (mstudiotexture_t*)data.get();
		texture->index = actualtextureindex + (texture->index - tmodel_textureindex);
	}
}

void Model::write(string fpath) {
	fstream fout = fstream(fpath.c_str(), std::ios::out | std::ios::binary);
	fout.write(data.getBuffer(), data.size());
}