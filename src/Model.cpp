#include "Model.h"
#include "util.h"
#include <iostream>
#include <fstream>
#include <string.h>
#include "lib/json.hpp"
#include "lib/md5.h"
#include <cstring>
#include "base_resample.h"
#include <algorithm>
#include "colors.h"
#include <unordered_map>

#ifdef _MSC_VER 
#define strncasecmp _strnicmp
#define strcasecmp _stricmp
#endif

using json::JSON;

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

		for (int k = 0; k < mod->nummesh; k++) {
			data.seek(mod->meshindex + k * sizeof(mstudiomesh_t));

			if (data.eom()) {
				cout << "ERROR: Failed to load mesh " + to_string(k) + " in model " + to_string(i) + "\n";
				return false;
			}

			mstudiomesh_t* mesh = (mstudiomesh_t*)data.get();

			data.seek(mesh->normindex + (mesh->numnorms*sizeof(vec3)) - 1);
			if (data.eom()) {
				cout << "ERROR: Failed to load normals for mesh " + to_string(k) + " in model " + to_string(i) + "\n";
				return false;
			}

			// vertex count = 3 for the first tri, then 1 for each additional tri (for strip/fan mode)
			data.seek(mesh->triindex + (mesh->numtris * sizeof(mstudiotrivert_t) + 2) - 1);
			if (data.eom()) {
				cout << "ERROR: Failed to load triangles for mesh " + to_string(k) + " in model " + to_string(i) + "\n";
				return false;
			}
		}
	}

	for (int i = 0; i < header->numseq; i++) {
		data.seek(header->seqindex + i*sizeof(mstudioseqdesc_t));
		if (data.eom()) {
			cout << "ERROR: Failed to load sequence " + to_string(i) + "/" + to_string(header->numseq) + "\n";
			return false;
		}

		mstudioseqdesc_t* seq = (mstudioseqdesc_t*)data.get();

		for (int k = 0; k < seq->numevents; k++) {
			data.seek(seq->eventindex + k*sizeof(mstudioevent_t));
			
			if (data.eom()) {
				cout << "ERROR: Failed to load event " + to_string(k) + "/" + to_string(seq->numevents) + " in sequence " + to_string(i) +"\n";
				return false;
			}

			mstudioevent_t* evt = (mstudioevent_t*)data.get();
		}

		data.seek(seq->animindex + (seq->numblends * header->numbones * sizeof(mstudioanim_t) * 6) - 1);
		if (data.eom()) {
			cout << "ERROR: Failed to load bone data for sequence " + to_string(i) + "/" + to_string(header->numseq) + "\n";
			return false;
		}
	}

	for (int i = 0; i < header->numbones; i++) {
		data.seek(header->boneindex + i * sizeof(mstudiobone_t));
		if (data.eom()) {
			cout << "ERROR: Failed to load sequence " + to_string(i) + "/" + to_string(header->numseq) + "\n";
			return false;
		}

		mstudiobone_t* bone = (mstudiobone_t*)data.get();
		if (bone->parent < -1 || bone->parent >= header->numbones) {
			cout << "ERROR: Bone " + to_string(i) + " has invalid parent " + to_string(bone->parent) + "\n";
			return false;
		}
	}

	for (int i = 0; i < header->numbonecontrollers; i++) {
		data.seek(header->bonecontrollerindex + i * sizeof(mstudiobonecontroller_t));
		if (data.eom()) {
			cout << "ERROR: Failed to load bone controller " + to_string(i) + "/" + to_string(header->numbonecontrollers) + "\n";
			return false;
		}

		mstudiobonecontroller_t* ctl = (mstudiobonecontroller_t*)data.get();
		if (ctl->bone < -1 || ctl->bone >= header->numbones) {
			cout << "ERROR: Controller " + to_string(i) + " references invalid bone " + to_string(ctl->bone) + "\n";
			return false;
		}
	}

	for (int i = 0; i < header->numhitboxes; i++) {
		data.seek(header->hitboxindex + i * sizeof(mstudiobbox_t));
		if (data.eom()) {
			cout << "ERROR: Failed to load bone controller " + to_string(i) + "/" + to_string(header->numhitboxes) + "\n";
			return false;
		}

		mstudiobbox_t* box = (mstudiobbox_t*)data.get();
		if (box->bone < -1 || box->bone >= header->numbones) {
			cout << "ERROR: Hitbox " + to_string(i) + " references invalid bone " + to_string(box->bone) + "\n";
			return false;
		}
	}

	for (int i = 0; i < header->numseqgroups; i++) {
		data.seek(header->seqgroupindex + i * sizeof(mstudioseqgroup_t));
		if (data.eom()) {
			cout << "ERROR: Failed to load sequence group " + to_string(i) + "/" + to_string(header->numseqgroups) + "\n";
			return false;
		}

		mstudioseqgroup_t* grp = (mstudioseqgroup_t*)data.get();
	}

	for (int i = 0; i < header->numtextures; i++) {
		data.seek(header->textureindex + i * sizeof(mstudiotexture_t));
		if (data.eom()) {
			cout << "ERROR: Failed to load texture " + to_string(i) + "/" + to_string(header->numtextures) + "\n";
			return false;
		}

		mstudiotexture_t* tex = (mstudiotexture_t*)data.get();
		data.seek(tex->index + (tex->width*tex->height + 256*3) - 1);
		if (data.eom()) {
			cout << "ERROR: Failed to load texture data " + to_string(i) + "/" + to_string(header->numtextures) + "\n";
			return false;
		}
	}

	for (int i = 0; i < header->numskinfamilies; i++) {
		data.seek(header->skinindex + i * sizeof(short)*header->numskinref);
		if (data.eom()) {
			cout << "ERROR: Failed to load skin family " + to_string(i) + "/" + to_string(header->numskinfamilies) + "\n";
			return false;
		}
	}

	for (int i = 0; i < header->numattachments; i++) {
		data.seek(header->attachmentindex + i * sizeof(mstudioattachment_t));
		if (data.eom()) {
			cout << "ERROR: Failed to load attachment " + to_string(i) + "/" + to_string(header->numattachments) + "\n";
			return false;
		}

		mstudioattachment_t* att = (mstudioattachment_t*)data.get();
		if (att->bone < -1 || att->bone >= header->numbones) {
			cout << "ERROR: Attachment " + to_string(i) + " references invalid bone " + to_string(att->bone) + "\n";
			return false;
		}
	}

	return true;
}

bool Model::isExtModel() {
	if (string(header->name).length() <= 0) {
		return true;
	}

	data.seek(header->bodypartindex);
	mstudiobodyparts_t* bod = (mstudiobodyparts_t*)data.get();
	return data.eom();
}

bool Model::isEmpty() {
	bool isEmptyModel = true;

	for (int b = 0; b < header->numbodyparts; b++) {
		// Try loading required model info
		data.seek(header->bodypartindex + b * sizeof(mstudiobodyparts_t));
		mstudiobodyparts_t* bod = (mstudiobodyparts_t*)data.get();

		for (int i = 0; i < bod->nummodels; i++) {
			data.seek(bod->modelindex + i * sizeof(mstudiomodel_t));
			mstudiomodel_t* mod = (mstudiomodel_t*)data.get();

			if (mod->nummesh != 0) {
				isEmptyModel = false;
				break;
			}
		}
	}

	return isEmptyModel;
}

bool Model::hasExternalTextures() {
	// textures aren't needed if the model has no triangles
	return header->numtextures == 0 && !isEmpty();
}

bool Model::hasExternalSequences() {
	return header->numseqgroups > 1;
}

void Model::insertData(void* src, size_t bytes) {
	if (bytes & 3) {
		printf("WARNING: Inserted unaligned data length\n");
	}

	data.insert(src, bytes);
	header = (studiohdr_t*)data.getBuffer();
}

void Model::removeData(size_t bytes) {
	if (bytes & 3) {
		printf("WARNING: Removed unaligned data length\n");
	}

	data.remove(bytes);
	header = (studiohdr_t*)data.getBuffer();
}

bool Model::mergeExternalTextures(bool deleteSource) {
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

	int lastSlash = tpath.find_last_of("/\\");
	string fname = tpath.substr(lastSlash + 1);
	cout << "Merging " << fname << "\n";

	Model tmodel(tpath);

	// copy info
	header->numtextures = tmodel.header->numtextures;
	header->numskinref = tmodel.header->numskinref;
	header->numskinfamilies = tmodel.header->numskinfamilies;

	// recalculate indexes
	size_t actualtextureindex = data.size();
	size_t tmodel_textureindex = tmodel.header->textureindex;
	size_t tmodel_skinindex = tmodel.header->skinindex;
	size_t tmodel_texturedataindex = tmodel.header->texturedataindex;
	header->textureindex = actualtextureindex;
	header->skinindex = actualtextureindex + (tmodel_skinindex - tmodel_textureindex);
	header->texturedataindex = actualtextureindex + (tmodel_texturedataindex - tmodel_textureindex);

	// texture data is at the end of the file, with the structures grouped together
	data.seek(0, SEEK_END);
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

	header->length = data.size();

	if (deleteSource)
		remove(tpath.c_str());

	return true;
}

#define MOVE_INDEX(val, afterIdx, delta) { \
	if (val >= afterIdx) { \
		val += delta; \
		/*cout << "Updated: " << #val << endl;*/ \
	} \
}

void Model::updateIndexes(int afterIdx, int delta) {
	// skeleton
	MOVE_INDEX(header->boneindex, afterIdx, delta);
	MOVE_INDEX(header->bonecontrollerindex, afterIdx, delta);
	MOVE_INDEX(header->attachmentindex, afterIdx, delta);
	MOVE_INDEX(header->hitboxindex, afterIdx, delta);

	// sequences
	MOVE_INDEX(header->seqindex, afterIdx, delta);
	MOVE_INDEX(header->seqgroupindex, afterIdx, delta);
	MOVE_INDEX(header->transitionindex, afterIdx, delta);
	for (int k = 0; k < header->numseq; k++) {
		data.seek(header->seqindex + k * sizeof(mstudioseqdesc_t));
		mstudioseqdesc_t* seq = (mstudioseqdesc_t*)data.get();

		MOVE_INDEX(seq->eventindex, afterIdx, delta);
		MOVE_INDEX(seq->animindex, afterIdx, delta);
		MOVE_INDEX(seq->pivotindex, afterIdx, delta);
		MOVE_INDEX(seq->automoveposindex, afterIdx, delta);		// unused?
		MOVE_INDEX(seq->automoveangleindex, afterIdx, delta);	// unused?
	}

	// meshes
	MOVE_INDEX(header->bodypartindex, afterIdx, delta);
	for (int i = 0; i < header->numbodyparts; i++) {
		data.seek(header->bodypartindex + i*sizeof(mstudiobodyparts_t));
		mstudiobodyparts_t* bod = (mstudiobodyparts_t*)data.get();
		MOVE_INDEX(bod->modelindex, afterIdx, delta);
		for (int k = 0; k < bod->nummodels; k++) {
			data.seek(bod->modelindex + k * sizeof(mstudiomodel_t));
			mstudiomodel_t* mod = (mstudiomodel_t*)data.get();

			MOVE_INDEX(mod->meshindex, afterIdx, delta);
			for (int j = 0; j < mod->nummesh; j++) {
				data.seek(mod->meshindex + j * sizeof(mstudiomesh_t));
				mstudiomesh_t* mesh = (mstudiomesh_t*)data.get();
				MOVE_INDEX(mesh->normindex, afterIdx, delta); // TODO: is this a file index?
				MOVE_INDEX(mesh->triindex, afterIdx, delta);
			}
			MOVE_INDEX(mod->normindex, afterIdx, delta);
			MOVE_INDEX(mod->norminfoindex, afterIdx, delta);
			MOVE_INDEX(mod->vertindex, afterIdx, delta);
			MOVE_INDEX(mod->vertinfoindex, afterIdx, delta);
		}
	}

	// textures
	MOVE_INDEX(header->textureindex, afterIdx, delta);
	for (int i = 0; i < header->numtextures; i++) {
		data.seek(header->textureindex + i * sizeof(mstudiotexture_t));
		mstudiotexture_t* texture = (mstudiotexture_t*)data.get();
		MOVE_INDEX(texture->index, afterIdx, delta);
	}
	MOVE_INDEX(header->skinindex, afterIdx, delta);
	MOVE_INDEX(header->texturedataindex, afterIdx, delta);

	// sounds (unused?)
	MOVE_INDEX(header->soundindex, afterIdx, delta);
	MOVE_INDEX(header->soundgroupindex, afterIdx, delta);

	header->length = data.size();
}

#define PRINT_TYPE_SIZE(name) printf("%-23s = %3d bytes\n", #name, sizeof(name))

void Model::printModelDataOrder() {
	struct DataChunk {
		int elementType; // allow shared indexes of the same type for deduplication
		int groupType; // for data summing
		string name;
		int offset;
		int size;
		int numShares; // how many times this chunk is shared
	};

	enum ELEMENT_TYPES {
		ET_NONE, // not an element or can't have shared pointers
		ET_EVT,
		ET_ANIM,
	};

	enum GROUP_TYPES {
		GT_NONE,
		GT_ANIM,
		GT_TEX,
		GT_MESH,
	};

	vector<DataChunk> chunks;

	// skeleton
	chunks.push_back({ ET_NONE, GT_NONE, "header", 0, sizeof(studiohdr_t)});
	chunks.push_back({ ET_NONE, GT_NONE, "bones", header->boneindex, header->numbones * (int)sizeof(mstudiobone_t)});
	chunks.push_back({ ET_NONE, GT_NONE, "bone controllers", header->bonecontrollerindex, header->numbonecontrollers * (int)sizeof(mstudiobonecontroller_t) });
	chunks.push_back({ ET_NONE, GT_NONE, "attachments", header->attachmentindex, header->numattachments * (int)sizeof(mstudioattachment_t) });
	chunks.push_back({ ET_NONE, GT_NONE, "hitboxes", header->hitboxindex, header->numhitboxes * (int)sizeof(mstudiobbox_t) });
	
	// sequences
	chunks.push_back({ ET_NONE, GT_ANIM, "sequence descriptions", header->seqindex, header->numseq * (int)sizeof(mstudioseqdesc_t) });
	chunks.push_back({ ET_NONE, GT_ANIM, "sequence groups", header->seqgroupindex, header->numseqgroups * (int)sizeof(mstudioseqgroup_t) });

	// sequences
	for (int k = 0; k < header->numseq; k++) {
		data.seek(header->seqindex + k * sizeof(mstudioseqdesc_t));
		mstudioseqdesc_t* seq = (mstudioseqdesc_t*)data.get();
		string seqname = "sequence " + to_string(k) + " -> ";

		chunks.push_back({ ET_EVT, GT_ANIM, seqname + "events", seq->eventindex, seq->numevents * (int)sizeof(mstudioevent_t)});
		
		data.seek(header->seqgroupindex);
		mstudioseqgroup_t* pseqgroup = (mstudioseqgroup_t*)data.get();
		data.seek(pseqgroup->data + seq->animindex);
		mstudioanim_t* panim = (mstudioanim_t*)data.get();
		int animDataSz = 0;

		for (int b = 0; b < seq->numblends; b++) {
			for (int i = 0; i < header->numbones; i++, panim++) {
				animDataSz += sizeof(mstudioanim_t);

				for (int j = 0; j < 6; j++) {
					if (panim->offset[j] == 0) {
						continue; // no data
					}

					mstudioanimvalue_t* pvaluehdr = (mstudioanimvalue_t*)((uint8_t*)panim + panim->offset[j]);
					animDataSz += (pvaluehdr->num.valid + 1) * sizeof(mstudioanimvalue_t);

					int frameCount = pvaluehdr->num.total;
					while (frameCount < seq->numframes) {
						pvaluehdr += pvaluehdr->num.valid + 1;
						frameCount += pvaluehdr->num.total;
						animDataSz += (pvaluehdr->num.valid + 1) * sizeof(mstudioanimvalue_t);
					}
				}
			}
		}
		
		chunks.push_back({ ET_ANIM, GT_ANIM, seqname + "frames", pseqgroup->data + seq->animindex, animDataSz });
		chunks.push_back({ ET_NONE, GT_ANIM, seqname + "pivots", seq->pivotindex, seq->numpivots * (int)sizeof(mstudiopivot_t) });
		//chunks.push_back({ ET_NONE, GT_ANIM, seqname + "automove positions", seq->automoveposindex, 0 });
		//chunks.push_back({ ET_NONE, GT_ANIM, seqname + "automove angles", seq->automoveangleindex, 0 });
	}

	// meshes
	chunks.push_back({ ET_NONE, GT_MESH, "body part infos", header->bodypartindex, header->numbodyparts * (int)sizeof(mstudiobodyparts_t) });

	for (int i = 0; i < header->numbodyparts; i++) {
		data.seek(header->bodypartindex + i * sizeof(mstudiobodyparts_t));
		mstudiobodyparts_t* bod = (mstudiobodyparts_t*)data.get();
		string bodname = "body " + to_string(i) + " -> ";

		chunks.push_back({ ET_NONE, GT_MESH, bodname + "model infos", bod->modelindex, bod->nummodels * (int)sizeof(mstudiomodel_t) });

		for (int k = 0; k < bod->nummodels; k++) {
			data.seek(bod->modelindex + k * sizeof(mstudiomodel_t));
			mstudiomodel_t* mod = (mstudiomodel_t*)data.get();
			string modname = bodname + "model " + to_string(k) + " -> ";

			chunks.push_back({ ET_NONE, GT_MESH, modname + "mesh infos", mod->meshindex, mod->nummesh * (int)sizeof(mstudiomesh_t) });

			for (int j = 0; j < mod->nummesh; j++) {
				data.seek(mod->meshindex + j * sizeof(mstudiomesh_t));
				mstudiomesh_t* mesh = (mstudiomesh_t*)data.get();
				string meshname = modname + "mesh " + to_string(j) + " -> ";

				// normals are stored in model array and referenced from triangle data
				//chunks.push_back({ meshname + "normals", mesh->normindex, mesh->numnorms * (int)sizeof(vec3) });

				data.seek(mesh->triindex);
				short* ptricmds = (short*)data.get();
				short* ptricmds_start = ptricmds;
				int p = 0;

				while (p = *(ptricmds++)) {
					if (p < 0) p = -p;
					for (; p > 0; p--, ptricmds += 4);
				}
				int triDataSz = (uint8_t*)ptricmds - (uint8_t*)ptricmds_start;
				chunks.push_back({ ET_NONE, GT_MESH, meshname + "triangles", mesh->triindex, triDataSz });
			}

			chunks.push_back({ ET_NONE, GT_MESH, modname + "normals", mod->normindex, mod->numnorms * (int)sizeof(vec3) });
			chunks.push_back({ ET_NONE, GT_MESH, modname + "normal bone indices", mod->norminfoindex, mod->numnorms * (int)sizeof(uint8_t) });
			chunks.push_back({ ET_NONE, GT_MESH, modname + "vertices", mod->vertindex, mod->numverts * (int)sizeof(vec3) });
			chunks.push_back({ ET_NONE, GT_MESH, modname + "vertex bone indices", mod->vertinfoindex, mod->numverts * (int)sizeof(uint8_t) });
		}
	}

	// textures
	chunks.push_back({ ET_NONE, GT_TEX, "texture infos", header->textureindex, header->numtextures * (int)sizeof(mstudiotexture_t) });

	for (int i = 0; i < header->numtextures; i++) {
		data.seek(header->textureindex + i * sizeof(mstudiotexture_t));
		mstudiotexture_t* texture = (mstudiotexture_t*)data.get();

		int texSize = texture->width * texture->height + 256 * 3;
		chunks.push_back({ ET_NONE, GT_TEX, "texture " + to_string(i), texture->index, texSize});
	}

	chunks.push_back({ ET_NONE, GT_TEX, "skins", header->skinindex, header->numskinfamilies * header->numskinref * (int)sizeof(short) });
	chunks.push_back({ ET_NONE, GT_TEX, "texture data index", header->texturedataindex, 0 });

	sort(chunks.begin(), chunks.end(), [](const DataChunk& a, const DataChunk& b) {
		return a.offset < b.offset;
	});

	vector<DataChunk> newChunks;
	for (int i = 0; i < chunks.size(); i++) {
		if (!chunks[i].size)
			continue;
			
		bool isShared = false;
		for (int k = 0; k < newChunks.size(); k++) {
			if (k == i)
				continue;
			if (chunks[i].offset == newChunks[k].offset && chunks[i].elementType == newChunks[k].elementType) {
				newChunks[k].numShares++;
				newChunks[k].size = max(newChunks[k].size, chunks[i].size);
				isShared = true;
				break;
			}
		}

		if (!isShared)
			newChunks.push_back(chunks[i]);
	}
	chunks = newChunks;

	/*
	printf("\nFile type sizes:\n");
	PRINT_TYPE_SIZE(studiohdr_t);
	PRINT_TYPE_SIZE(studioseqhdr_t);
	PRINT_TYPE_SIZE(mstudiobone_t);
	PRINT_TYPE_SIZE(mstudiobonecontroller_t);
	PRINT_TYPE_SIZE(mstudiobbox_t);
	PRINT_TYPE_SIZE(mstudioseqgroup_t);
	PRINT_TYPE_SIZE(mstudioseqdesc_t);
	PRINT_TYPE_SIZE(mstudioevent_t);
	PRINT_TYPE_SIZE(mstudiopivot_t);
	PRINT_TYPE_SIZE(mstudioattachment_t);
	PRINT_TYPE_SIZE(mstudioanim_t);
	PRINT_TYPE_SIZE(mstudioanimvalue_t);
	PRINT_TYPE_SIZE(mstudiobodyparts_t);
	PRINT_TYPE_SIZE(mstudiotexture_t);
	PRINT_TYPE_SIZE(mstudiomodel_t);
	PRINT_TYPE_SIZE(mstudiomesh_t);
	PRINT_TYPE_SIZE(mstudiotrivert_t);
	*/

	int totalGap = 0;
	int totalOverlap = 0;
	int totalAlignmentPadding = 0;
	int totalAnimationBytes = 0;
	int totalTextureBytes = 0;
	int totalMeshBytes = 0;

	printf("\nFile layout:\n");
	for (int i = 0; i < chunks.size(); i++) {
		DataChunk& chunk = chunks[i];

		string overlap = "";
		if (i > 0) {
			DataChunk& lastChunk = chunks[i-1];
			int overlapBytes = lastChunk.offset + lastChunk.size - chunk.offset;
			if (overlapBytes > 0 && lastChunk.size > 0 && chunk.size > 0) {
				overlap = " (OVERLAP " + to_string(overlapBytes) + " BYTES!)";
				totalOverlap += overlapBytes;
			}
			else if (overlapBytes < 0) {
				overlapBytes = -overlapBytes;
				if (overlapBytes < 4 && (chunk.offset - overlapBytes & 3)) {
					// expected alignment of index pointer
					overlap = " (+" + to_string(overlapBytes) + " byte alignment)";
					totalAlignmentPadding += overlapBytes;
				}
				else {
					totalGap += overlapBytes;
					overlap = " (GAP OF " + to_string(overlapBytes) + " BYTES!)";
				}
			}
		}

		switch (chunk.groupType) {
		case GT_ANIM:
			totalAnimationBytes += chunk.size;
			break;
		case GT_MESH:
			totalMeshBytes += chunk.size;
			break;
		case GT_TEX:
			totalTextureBytes += chunk.size;
			break;
		}

		string shares = "";
		if (chunk.numShares) {
			shares = " (" + to_string(chunk.numShares+1) + " shares)";
		}

		const char* unlaigned = (chunk.offset & 3) ? " (UNALIGNED)" : "";

		printf("%7d (%6d bytes) : %s%s%s%s\n", chunk.offset,
			chunk.size, chunk.name.c_str(), shares.c_str(), unlaigned, overlap.c_str());
	}
	printf("\nTotal overlap bytes   : %d\n", totalOverlap);
	printf("Total mystery bytes   : %d\n", totalGap);
	printf("Total alignment bytes : %d\n\n", totalAlignmentPadding);

	int miscBytes = data.size() - (totalMeshBytes + totalAnimationBytes + totalTextureBytes);
	printf("Mesh data      : %.1f KB\n", totalMeshBytes / 1024.0f);
	printf("Animation data : %.1f KB\n", totalAnimationBytes / 1024.0f);
	printf("Texture data   : %.1f KB\n", totalTextureBytes / 1024.0f);
	printf("Misc data      : %.1f KB\n\n", miscBytes / 1024.0f);
}

bool Model::mergeExternalSequences(bool deleteSource) {
	if (!hasExternalSequences()) {
		cout << "No external sequences to merge\n";
		return false;
	}

	int lastDot = fpath.find_last_of(".");
	string ext = fpath.substr(lastDot);
	string basepath = fpath.substr(0, lastDot);

	// save external animations to the end of the file.
	data.seek(0, SEEK_END);

	// save old values before they're overwritten in index updates
	int* oldanimindexes = new int[header->numseq];
	for (int k = 0; k < header->numseq; k++) {
		data.seek(header->seqindex + k * sizeof(mstudioseqdesc_t));
		mstudioseqdesc_t* seq = (mstudioseqdesc_t*)data.get();
		oldanimindexes[k] = seq->animindex;
	}

	for (int i = 1; i < header->numseqgroups; i++)
	{
		string suffix = i < 10 ? "0" + to_string(i) : to_string(i);
		string spath = basepath + suffix + ext;

		if (!fileExists(spath)) {
			cout << "External sequence model not found: " << spath << endl;
			return false;
		}

		int lastSlash = spath.find_last_of("/\\");
		string fname = spath.substr(lastSlash + 1);
		cout << "Merging " << fname << "\n";

		Model smodel(spath);

		// Sequence models contain a header followed by animation data.
		// This will append those animations after the primary model's animations.
		data.seek(header->seqindex);
		smodel.data.seek(sizeof(studioseqhdr_t));
		size_t insertOffset = data.tell();
		size_t animCopySize = smodel.data.size() - sizeof(studioseqhdr_t);
		insertData(smodel.data.get(), animCopySize);
		updateIndexes(insertOffset, animCopySize);

		// update indexes for the merged animations
		for (int k = 0; k < header->numseq; k++) {
			data.seek(header->seqindex + k * sizeof(mstudioseqdesc_t));
			mstudioseqdesc_t* seq = (mstudioseqdesc_t*)data.get();

			if (seq->seqgroup != i)
				continue;

			seq->animindex = insertOffset + (oldanimindexes[k] - sizeof(studioseqhdr_t));
			seq->seqgroup = 0;
		}

		if (deleteSource)
			remove(spath.c_str());
	}

	delete[] oldanimindexes;

	// remove infos for the merged sequence groups
	data.seek(header->seqgroupindex + sizeof(mstudioseqgroup_t));
	size_t removeOffset = data.tell();
	int removeBytes = sizeof(mstudioseqgroup_t) * (header->numseqgroups - 1);
	removeData(removeBytes);
	header->numseqgroups = 1;
	updateIndexes(removeOffset, -removeBytes);

	return true;
}

bool Model::cropTexture(string cropName, int newWidth, int newHeight) {
	for (int i = 0; i < header->numtextures; i++) {
		data.seek(header->textureindex + i * sizeof(mstudiotexture_t));
		mstudiotexture_t* texture = (mstudiotexture_t*)data.get();
		string name = texture->name;

		if (string(texture->name) != cropName) {
			continue;
		}

		cout << "Cropping " << cropName << " from " << texture->width << "x" << texture->height <<
			" to " << newWidth << "x" << newHeight << endl;

		data.seek(texture->index);
		int oldSize = texture->width * texture->height;
		int newSize = newWidth * newHeight;
		int palSize = 256 * 3;
		uint8_t* oldTexData = new uint8_t[oldSize];
		uint8_t* palette = new uint8_t[palSize];
		uint8_t* newTexData = new uint8_t[newSize];
		data.read(oldTexData, oldSize);
		data.read(palette, palSize);
		
		for (int y = 0; y < newHeight; y++) {
			for (int x = 0; x < newWidth; x++) {
				int oldY = y >= texture->height ? texture->height-1 : y;
				int oldX = x >= texture->width ? texture->width -1 : x;
				newTexData[y * newWidth + x] = oldTexData[oldY*texture->width + oldX];
			}
		}

		data.seek(texture->index);
		data.write(newTexData, newSize);
		data.write(palette, palSize);

		texture->width = newWidth;
		texture->height = newHeight;

		size_t removeAt = data.tell();
		int removeBytes = oldSize - newSize;
		removeData(removeBytes);
		updateIndexes(removeAt, -removeBytes);

		header->length = data.size();

		return true;
	}

	cout << "ERROR: No texture found with name '" << cropName << "'\n";
	return false;
}

bool Model::resizeTexture(string texName, int newWidth, int newHeight) {
	for (int i = 0; i < header->numtextures; i++) {
		data.seek(header->textureindex + i * sizeof(mstudiotexture_t));
		mstudiotexture_t* texture = (mstudiotexture_t*)data.get();
		string name = texture->name;

		if (name != texName) {
			continue;
		}

		cout << "Resizing " << texName << " from " << texture->width << "x" << texture->height <<
			" to " << newWidth << "x" << newHeight << endl;

		float scaleX = (float)newWidth / (float)texture->width;
		float scaleY = (float)newHeight / (float)texture->height;

		data.seek(texture->index);
		int oldSize = texture->width * texture->height;
		int newSize = newWidth * newHeight;
		int palSize = 256 * 3;

		if (newSize > oldSize) {
			cout << "Resize failed. New texture size is larger than current size.\n";
			return false;
		}

		if (newSize % 8) {
			cout << "Resize failed. New texture pixel count is not divisible by 8.\n";
			return false;
		}

		uint8_t* oldTexData = new uint8_t[oldSize];
		uint8_t* palette = new uint8_t[palSize];
		uint8_t* newTexData = new uint8_t[newSize];
		data.read(oldTexData, oldSize);
		data.read(palette, palSize);

		uint8_t* oldImage = new uint8_t[oldSize * 3];
		uint8_t* newImage = new uint8_t[newSize * 3];
		for (int y = 0; y < texture->height; y++) {
			for (int x = 0; x < texture->width; x++) {
				int idx = y * texture->width + x;
				oldImage[idx*3 + 0] = palette[oldTexData[idx]*3 + 0];
				oldImage[idx*3 + 1] = palette[oldTexData[idx]*3 + 1];
				oldImage[idx*3 + 2] = palette[oldTexData[idx]*3 + 2];
			}
		}
		
		string lowername = toLowerCase(name);
		if (lowername.find("remap") == 0 || lowername.find("dm_base") == 0) {
			// remappable textures must use the same palette exactly
			float scalex = texture->width / (float)newWidth;
			float scaley = texture->height / (float)newHeight;

			int maxSrcIdx = (texture->width * texture->height) - 1;

			for (int y = 0; y < newHeight; y++) {
				for (int x = 0; x < newWidth; x++) {
					int srcIdx = clamp((int)(y*scaley) * texture->width + x*scalex, 0, maxSrcIdx);
					int dstIdx = y * newWidth + x;
					newTexData[dstIdx] = oldTexData[srcIdx];
				}
			}
		}
		else {
			base::ResampleImage24((uint8_t*)oldImage, texture->width, texture->height,
				(uint8_t*)newImage, newWidth, newHeight,
				base::KernelType::KernelTypeLanczos2);

			for (int y = 0; y < newHeight; y++) {
				for (int x = 0; x < newWidth; x++) {
					int idx = y * newWidth + x;
					int r = newImage[idx * 3 + 0];
					int g = newImage[idx * 3 + 1];
					int b = newImage[idx * 3 + 2];

					// use closest color in existing palette
					int bestDiff = 99999;
					int bestIdx = 0;
					for (int k = 0; k < 256; k++) {
						int pr = palette[k * 3 + 0];
						int pg = palette[k * 3 + 1];
						int pb = palette[k * 3 + 2];

						int diff = abs(pr - r) + abs(pg - g) + abs(pb - b);
						if (diff < bestDiff) {
							bestDiff = diff;
							bestIdx = k;
						}
					}

					newTexData[idx] = bestIdx;
				}
			}
		}

		data.seek(texture->index);
		data.write(newTexData, newSize);
		data.write(palette, palSize);

		texture->width = newWidth;
		texture->height = newHeight;

		size_t removeAt = data.tell();
		int removeBytes = oldSize - newSize;
		removeData(removeBytes);
		updateIndexes(removeAt, -removeBytes);

		header->length = data.size();

		for (int b = 0; b < header->numbodyparts; b++) {
			// Try loading required model info
			data.seek(header->bodypartindex + b * sizeof(mstudiobodyparts_t));
			mstudiobodyparts_t* bod = (mstudiobodyparts_t*)data.get();

			for (int m = 0; m < bod->nummodels; m++) {
				data.seek(bod->modelindex + m * sizeof(mstudiomodel_t));
				mstudiomodel_t* mod = (mstudiomodel_t*)data.get();

				if (data.eom()) {
					cout << "ERROR: Failed to load body " + to_string(m) + "/" + to_string(bod->nummodels) + "\n";
					return false;
				}

				for (int k = 0; k < mod->nummesh; k++) {
					data.seek(mod->meshindex + k * sizeof(mstudiomesh_t));

					if (data.eom()) {
						cout << "ERROR: Failed to load mesh " + to_string(k) + " in model " + to_string(m) + "\n";
						return false;
					}

					mstudiomesh_t* mesh = (mstudiomesh_t*)data.get();

					data.seek(header->skinindex);
					short* skins = (short*)data.get();

					short remappedSkin = skins[mesh->skinref];
					if (remappedSkin < 0 || remappedSkin >= header->numtextures) {
						remappedSkin = mesh->skinref;
					}

					if (remappedSkin != i) {
						continue;
					}

					// Update texture coordinates
					data.seek(mesh->triindex);
					short* ptricmds = (short*)data.get();
					int p = 0;

					while (p = *(ptricmds++)) {
						if (p < 0) {
							p = -p;
						}

						// There is data loss here because texture coordinates are stored as pixel offsets.
						// Textures may shift slightly at low resolutions.
						for (; p > 0; p--, ptricmds += 4) {
							ptricmds[2] = roundf((float)ptricmds[2] * scaleX);
							ptricmds[3] = roundf((float)ptricmds[3] * scaleY);
						}
					}
				}
			}
		}

		return true;
	}

	cout << "ERROR: No texture found with name '" << texName << "'\n";
	return false;
}

bool Model::renameTexture(string oldName, string newName) {
	for (int i = 0; i < header->numtextures; i++) {
		data.seek(header->textureindex + i * sizeof(mstudiotexture_t));
		mstudiotexture_t* texture = (mstudiotexture_t*)data.get();

		if (string(texture->name) == oldName) {
			strncpy(texture->name, newName.c_str(), 64);
			return true;
		}
	}

	cout << "ERROR: No texture found with name '" << oldName << "'\n";
	return false;
}

void Model::write(string fpath) {
	fstream fout = fstream(fpath.c_str(), std::ios::out | std::ios::binary);
	fout.write(data.getBuffer(), data.size());
	cout << "Wrote " << fpath << " (" << data.size() << " bytes)\n";
}

vector<vec3> Model::getVertexes() {
	int totalVerts = 0;

	for (int k = 0; k < header->numbodyparts; k++) {
		data.seek(header->bodypartindex + k * sizeof(mstudiobodyparts_t));
		mstudiobodyparts_t* bod = (mstudiobodyparts_t*)data.get();

		for (int i = 0; i < bod->nummodels; i++) {
			data.seek(bod->modelindex + i * sizeof(mstudiomodel_t));
			mstudiomodel_t* mod = (mstudiomodel_t*)data.get();
			totalVerts += mod->numverts;
		}
	}

	
	vector<vec3> verts;

	for (int k = 0; k < header->numbodyparts; k++) {
		data.seek(header->bodypartindex + k*sizeof(mstudiobodyparts_t));
		mstudiobodyparts_t* bod = (mstudiobodyparts_t*)data.get();

		for (int i = 0; i < bod->nummodels; i++) {
			data.seek(bod->modelindex + i * sizeof(mstudiomodel_t));
			mstudiomodel_t* mod = (mstudiomodel_t*)data.get();
			data.seek(mod->vertindex);
			vec3* vertSrc = (vec3 * )data.get();

			verts.reserve(mod->numverts);
			for (int v = 0; v < mod->numverts; v++) {
				verts.push_back(vertSrc[v]);
			}
		}
	}

	return verts;
}

void Model::dump_info(string outputPath) {
	JSON obj = json::Object();

	obj["seq_groups"] = to_string(header->numseqgroups);
	obj["t_model"] = hasExternalTextures();

	if (hasExternalTextures())
		mergeExternalTextures(false);
	if (hasExternalSequences())
		mergeExternalSequences(false);

	obj["size"] = data.size();

	MD5 hash = MD5();
	hash.add(data.getBuffer(), data.size());
	obj["md5"] = hash.getHash();

	JSON jbodies = json::Array();
	for (int k = 0; k < header->numbodyparts; k++) {
		data.seek(header->bodypartindex + k * sizeof(mstudiobodyparts_t));
		mstudiobodyparts_t* bod = (mstudiobodyparts_t*)data.get();

		JSON jbody = json::Object();
		JSON jmodels = json::Array();

		for (int i = 0; i < bod->nummodels; i++) {
			data.seek(bod->modelindex + i * sizeof(mstudiomodel_t));
			mstudiomodel_t* mod = (mstudiomodel_t*)data.get();

			JSON jmodel = json::Object();
			int polyCount = 0;

			for (int i = 0; i < mod->nummesh; i++) {
				data.seek(mod->meshindex + i * sizeof(mstudiomesh_t));
				mstudiomesh_t* mesh = (mstudiomesh_t*)data.get();
				polyCount += mesh->numtris;
			}

			jmodel["name"] = sanitize_string(mod->name);
			jmodel["polys"] = polyCount;
			jmodel["verts"] = mod->numverts;
			jmodels.append(jmodel);
		}

		jbody["name"] = sanitize_string(bod->name);
		jbody["models"] = jmodels;
		jbodies.append(jbody);
	}

	JSON jtextures = json::Array();
	for (int i = 0; i < header->numtextures; i++) {
		data.seek(header->textureindex + i * sizeof(mstudiotexture_t));
		mstudiotexture_t* texture = (mstudiotexture_t*)data.get();

		JSON jtexture = json::Object();
		jtexture["name"] = sanitize_string(texture->name);
		jtexture["flags"] = texture->flags;
		jtexture["width"] = texture->width;
		jtexture["height"] = texture->height;
		jtextures.append(jtexture);
	}

	JSON jseqs = json::Array();
	JSON jevents = json::Array();
	for (int i = 0; i < header->numseq; i++) {
		data.seek(header->seqindex + i * sizeof(mstudioseqdesc_t));
		mstudioseqdesc_t* seq = (mstudioseqdesc_t*)data.get();

		
		for (int k = 0; k < seq->numevents; k++) {
			data.seek(seq->eventindex + k * sizeof(mstudioevent_t));
			mstudioevent_t* evt = (mstudioevent_t*)data.get();

			JSON jevent = json::Object();
			jevent["event"] = evt->event;
			jevent["type"] = evt->type;
			jevent["sequence"] = i;
			jevent["frame"] = evt->frame;
			jevent["options"] = sanitize_string(evt->options);
			jevents.append(jevent);
		}

		JSON jseq = json::Object();
		jseq["name"] = sanitize_string(seq->label);
		jseq["fps"] = seq->fps;
		jseq["frames"] = seq->numframes;
		jseqs.append(jseq);
	}

	JSON jskel = json::Array();
	for (int i = 0; i < header->numbones; i++) {
		data.seek(header->boneindex + i * sizeof(mstudiobone_t));
		mstudiobone_t* bone = (mstudiobone_t*)data.get();

		JSON jbone = json::Object();
		jbone["name"] = sanitize_string(bone->name);
		jbone["parent"] = bone->parent;
		jskel.append(jbone);
	}

	JSON jattachments = json::Array();
	for (int i = 0; i < header->numattachments; i++) {
		data.seek(header->attachmentindex + i * sizeof(mstudioattachment_t));
		mstudioattachment_t* att = (mstudioattachment_t*)data.get();

		JSON jatt = json::Object();
		jatt["name"] = sanitize_string(att->name);
		jatt["bone"] = att->bone;
		jatt["type"] = att->type;
		jattachments.append(jatt);
	}

	JSON jctls = json::Array();
	for (int i = 0; i < header->numbonecontrollers; i++) {
		data.seek(header->bonecontrollerindex + i * sizeof(mstudiobonecontroller_t));
		mstudiobonecontroller_t* ctl = (mstudiobonecontroller_t*)data.get();

		JSON jctl = json::Object();
		jctl["type"] = ctl->type;
		jctl["index"] = ctl->index;
		jctl["bone"] = ctl->bone;
		jctl["start"] = ctl->start;
		jctl["end"] = ctl->end;
		jctl["rest"] = ctl->rest;
		jctls.append(jctl);
	}

	obj["name"] = sanitize_string(header->name);
	obj["textures"] = jtextures;
	obj["bodies"] = jbodies;
	obj["sequences"] = jseqs;
	obj["events"] = jevents;
	obj["skeleton"] = jskel;
	obj["controllers"] = jctls;
	obj["attachments"] = jattachments;
	obj["skins"] = header->numskinfamilies;
	obj["id"] = header->id;
	obj["version"] = header->version;

	ofstream fout;
	fout.open(outputPath);
	fout << obj << endl;
	fout.close();
}

int Model::wavify() {
	const char* nonstandard_audio_formats[21] = {
		"aiff", "asf", "asx", "au", "dls", "flac",
		"fsb", "it", "m3u", "mid", "midi", "mod",
		"mp2", "ogg", "pls", "s3m", "vag", "wax",
		"wma", "xm", "xma"
	};
	int fmt_len = sizeof(nonstandard_audio_formats) / sizeof(const char*);

	int numConverted = 0;

	for (int i = 0; i < header->numseq; i++) {
		data.seek(header->seqindex + i * sizeof(mstudioseqdesc_t));
		mstudioseqdesc_t* seq = (mstudioseqdesc_t*)data.get();

		for (int k = 0; k < seq->numevents; k++) {
			data.seek(seq->eventindex + k * sizeof(mstudioevent_t));
			mstudioevent_t* evt = (mstudioevent_t*)data.get();
			std::string val = sanitize_string(evt->options);

			int lastdot = val.rfind(".");

			if (lastdot != -1) {
				std::string ext = val.substr(lastdot+1);

				bool isBadAudio = false;
				for (int k = 0; k < fmt_len; k++) {
					if (!strcasecmp(nonstandard_audio_formats[k], ext.c_str())) {
						isBadAudio = true;
						break;
					}
				}

				if (isBadAudio) {
					val = val.substr(0, lastdot + 1) + "wav";
					strncpy(evt->options, val.c_str(), 64);
					evt->options[63] = 0;
					numConverted++;
				}
			}
		}
	}

	return numConverted;
}

bool Model::hackshade() {
	data.seek(header->skinindex);
	short* skins = (short*)data.get();

	data.seek(header->textureindex);
	mstudiotexture_t* textures = (mstudiotexture_t*)data.get();
	int numFlatshades = 0;

	for (int b = 0; b < header->numbodyparts; b++) {
		// Try loading required model info
		data.seek(header->bodypartindex + b * sizeof(mstudiobodyparts_t));
		mstudiobodyparts_t* bod = (mstudiobodyparts_t*)data.get();

		for (int i = 0; i < bod->nummodels; i++) {
			data.seek(bod->modelindex + i * sizeof(mstudiomodel_t));
			mstudiomodel_t* mod = (mstudiomodel_t*)data.get();

			data.seek(mod->normindex);
			vec3* pstudionorms = (vec3*)data.get();

			for (int k = 0; k < mod->nummesh; k++) {
				data.seek(mod->meshindex + k * sizeof(mstudiomesh_t));
				mstudiomesh_t* mesh = (mstudiomesh_t*)data.get();

				short remappedSkin = skins[mesh->skinref];
				if (remappedSkin < 0 || remappedSkin >= header->numtextures) {
					remappedSkin = mesh->skinref;
				}

				int flags = textures[remappedSkin].flags;

				if (!(flags & (STUDIO_NF_FLATSHADE | STUDIO_NF_FULLBRIGHT) )) {
					continue; // not flat shaded or fullbright
				}

				if (flags & STUDIO_NF_CHROME) {
					printf("Warning: Chrome textures can't be flatshaded (%s)\n",
						textures[remappedSkin].name);
					continue; // changing normals will break the chrome effect
				}

				if (flags & STUDIO_NF_FULLBRIGHT) {
					printf("Warning: Fullbright texture converted to flatshade (%s)\n",
						textures[remappedSkin].name);
				}

				data.seek(mesh->triindex);
				short* ptricmds = (short*)data.get();
				int p;

				while (p = *(ptricmds++)) {
					if (p < 0) {
						p = -p;
					}

					for (; p > 0; p--, ptricmds += 4) {
						pstudionorms[ptricmds[1]] = vec3(0,0,0);
					}
				}
			}
		}
	}

	if (numFlatshades)
		cout << "Applied flatshade normals for " << numFlatshades << " / " << header->numtextures << " textures" << endl;

	return numFlatshades != 0;
}

mstudioanim_t* Model::getAnimFrames(int sequence) {
	data.seek(header->seqindex + sequence * sizeof(mstudioseqdesc_t));
	mstudioseqdesc_t* seq = (mstudioseqdesc_t*)data.get();

	if (seq->seqgroup > 0) {
		return NULL;
	}

	data.seek(header->seqgroupindex);
	mstudioseqgroup_t* pseqgroup = (mstudioseqgroup_t*)data.get();
	data.seek(pseqgroup->data + seq->animindex);
	return (mstudioanim_t*)data.get();
}

bool Model::padAnimation(int sequence, int newFrameCount) {
	data.seek(header->seqindex + sequence * sizeof(mstudioseqdesc_t));
	mstudioseqdesc_t* seq = (mstudioseqdesc_t*)data.get();

	if (seq->seqgroup > 0) {
		printf("External sequence padding not supported\n");
		return false;
	}

	int padFrames = newFrameCount - seq->numframes;
	if (padFrames < 0 || padFrames > 255) {
		printf("Can't add %d frames of padding to animation\n", padFrames);
		return false;
	}

	mstudioanim_t* panim = getAnimFrames(sequence);
	
	for (int b = 0; b < seq->numblends; b++) {
		for (int i = 0; i < header->numbones; i++, panim++) {
			for (int j = 0; j < 6; j++) {
				if (panim->offset[j] == 0) {
					continue; // no data
				}

				mstudioanimvalue_t* pvaluehdr = (mstudioanimvalue_t*)((uint8_t*)panim + panim->offset[j]);

				int frameCount = pvaluehdr->num.total;
				while (frameCount < seq->numframes) {
					pvaluehdr += pvaluehdr->num.valid + 1; // 1 = skip the header too
					frameCount += pvaluehdr->num.total;
				}

				// The last value of "valid" is repeated until "total" is reached. No need to add data.
				pvaluehdr->num.total += padFrames;

				// unless we overflow (TODO: find a model that does this)
				if ((int)pvaluehdr->num.total + padFrames > 255) {
					printf("Overflowed animation chunk! Model will be broken.\n");
					return false;
				}
			}
		}
	}

	seq->numframes += padFrames;
	return true;
}

bool Model::port_sc_animations_to_hl() {
	// duplicate existing anims
	mstudioseqdesc_t* originalSeqs = new mstudioseqdesc_t[header->numseq];
	memset(originalSeqs, 0, header->numseq * sizeof(mstudioseqdesc_t));
	data.seek(header->seqindex);
	memcpy(originalSeqs, data.get(), header->numseq * sizeof(mstudioseqdesc_t));

	// reserve space for new anims 
	int additionalSequenceCount = 11; // (6 HL anims, 2x alt idles, 2x alt shotty reload, alt jump)
	data.seek(header->seqindex + header->numseq * sizeof(mstudioseqdesc_t));
	insertData(originalSeqs, additionalSequenceCount * sizeof(mstudioseqdesc_t)); // dummy data
	int originalSeqCount = header->numseq;
	header->numseq += additionalSequenceCount;

	// Map a half-life animation index to a sven co-op index. All idles map to 1 because sven only
	// uses the 2nd idle animation. Modelers sometimes add special animations for index 0 and 2.
	int sc_to_hl_index[77] = {
		1, 1, 1, 3, 4, 5, 6, 7, 8, 9, 10, 11, 3, 4, 64, 65, 52, 53, 12, 13, 14, 15, 16, 17, 18,
		20, 21, 23, 24, 42, 43, 45, 46, 48, 49, 52, 53, 56, 57, 60, 61, 64, 65, 68, 69, 72, 73,
		75, 76, 78, 79, 82, 83, 86, 87, 90, 91, 94, 95, 97, 98, 100, 101, 103, 104, 106, 107,
		109, 110, 112, 113, 118, 119, 177, 178, 179, 180
	};

	int animBufferCount = max(256, header->numseq); // so no need for overflow checks
	mstudioseqdesc_t* reordered_seqs = new mstudioseqdesc_t[animBufferCount];
	bool* addedAnims = new bool[animBufferCount];
	memset(reordered_seqs, 0, animBufferCount * sizeof(mstudioseqdesc_t));
	memset(addedAnims, 0, animBufferCount * sizeof(bool));

	int appendIdx = 0;
	for (int i = 0; i < 77 && i < originalSeqCount; i++) {
		int srcIdx = sc_to_hl_index[i];

		if (srcIdx >= originalSeqCount) {
			srcIdx = min(originalSeqCount, 1);
		}

		reordered_seqs[appendIdx++] = originalSeqs[srcIdx];
		addedAnims[srcIdx] = true;
	}

	// add these animations again because the HL indexed ones will be cut short to slow them down
	// for vanilla HL servers. The duplicates will have the full length and be used by mods.
	addedAnims[8] = false; // jump
	addedAnims[65] = false; // shoot shotgun
	addedAnims[69] = false; // shoot shotgun (crouched)

	bool unexpectedAnims = false;

	// append all other animations that haven't been added yet
	for (int i = 0; i < originalSeqCount; i++) {
		if (!addedAnims[i]) {
			if (appendIdx >= header->numseq) {
				unexpectedAnims = true;
				break;
			}
			reordered_seqs[appendIdx++] = originalSeqs[i];
		}
	}

	// failsafe
	while (appendIdx < header->numseq) {
		unexpectedAnims = true;
		reordered_seqs[appendIdx++] = originalSeqs[0];
	}

	if (unexpectedAnims) {
		printf("The source model had unexpected animations. Expect incorrect output.\n");
	}

	// rename the unused HL animations. In HL these are all duplicates of existing anims
	// except for shoot_1 which is a pistol shot with a slightly different aim direction.
	const int labelSz = sizeof(reordered_seqs[0].label);
	strncpy(reordered_seqs[12].label, "run", labelSz);
	strncpy(reordered_seqs[13].label, "walk", labelSz);
	strncpy(reordered_seqs[14].label, "aim_2", labelSz);
	strncpy(reordered_seqs[15].label, "shoot_2", labelSz);
	strncpy(reordered_seqs[16].label, "aim_1", labelSz);
	strncpy(reordered_seqs[17].label, "shoot_1", labelSz);

	// rename duplicated "idle" anims to match HL defaults
	strncpy(reordered_seqs[0].label, "look_idle", labelSz);
	strncpy(reordered_seqs[2].label, "deep_idle", labelSz);
	strncpy(reordered_seqs[77].label, "look_idle2", labelSz);
	strncpy(reordered_seqs[78].label, "deep_idle2", labelSz);

	// set correct activities
	ModelType* mtype = getModelType(PMODEL_HALF_LIFE);
	if (mtype) {
		for (int i = 0; i < mtype->anims.size() && i < header->numseq; i++) {
			reordered_seqs[i].activity = mtype->anims[i].activity;
		}
		for (int i = mtype->anims.size(); i < header->numseq; i++) {
			reordered_seqs[i].activity = 0;
		}
	}
	else {
		printf("Failed to get half-life mode info\n");
	}

	// shorten jump animation to slow it down. The last ~100 frames aren't critical
	// (flailing arms for a few seconds). It looks much worse to have the model finish
	// the jump at lightning speed.
	reordered_seqs[8].numframes = reordered_seqs[8].numframes * (34.0f / 128.0f) + 0.5f;

	// shotgun shoot animations also need to be slowed down. Cutting this short
	// removes the cocking after recoil. Not great but the speedup is worse.
	reordered_seqs[42].numframes = reordered_seqs[42].numframes * (8.0f / 17.0f) + 0.5f;
	reordered_seqs[44].numframes = reordered_seqs[44].numframes * (8.0f / 17.0f) + 0.5f;

	// Other weapon anims are not identical speeds (gauss, mp5, hornetgun) but that's less noticeable
	// than animation cutting will be. Snark petting is noticeably slower, but doubling its speed by
	// looping it will make it too fast. It should be resampled to reduce frame count but that will
	// add a lot of complication for low payoff.

	// rename alternate versions of animations that were not cut short
	strncpy(reordered_seqs[79].label, "jump2", labelSz);
	strncpy(reordered_seqs[109].label, "ref_shoot_shotgun2", labelSz);
	strncpy(reordered_seqs[112].label, "crouch_shoot_shotgun2", labelSz);

	data.seek(header->seqindex);
	memcpy(data.get(), reordered_seqs, header->numseq * sizeof(mstudioseqdesc_t));

	int additionalSeqDataSz = additionalSequenceCount * sizeof(mstudioseqdesc_t);
	updateIndexes(header->seqindex + originalSeqCount * sizeof(mstudioseqdesc_t), additionalSeqDataSz);

	// speed up animations by repeating the last frame. The xbow is noticeably too slow, but the
	// others could probably be skipped. No harm increasing their frame count to match HL.
	padAnimation(70, reordered_seqs[70].numframes * (31.0f /  7.0f) + 0.5f); // xbow shoot
	padAnimation(72, reordered_seqs[72].numframes * (31.0f /  7.0f) + 0.5f); // xbow shoot (crouched)
	padAnimation(54, reordered_seqs[54].numframes * (16.0f / 11.0f) + 0.5f); // rpg shoot
	padAnimation(56, reordered_seqs[56].numframes * (16.0f / 11.0f) + 0.5f); // rpg shoot (crouched)
	padAnimation(62, reordered_seqs[62].numframes * (13.0f / 11.0f) + 0.5f); // throw snark
	padAnimation(64, reordered_seqs[64].numframes * (13.0f / 11.0f) + 0.5f); // throw snark (crouched)

	// How to test: thirdperson; maxplayers 2; sv_cheats 1; map crossfire

	delete[] originalSeqs;
	delete[] reordered_seqs;
	delete[] addedAnims;

	return true;
}

void Model::downscale_textures(int maxPixels) {
	for (int i = 0; i < header->numtextures; i++) {
		data.seek(header->textureindex + i * sizeof(mstudiotexture_t));
		mstudiotexture_t* texture = (mstudiotexture_t*)data.get();

		if (texture->width * texture->height <= maxPixels)
			continue;

		float scale = sqrt((float)maxPixels / (texture->width * texture->height));
		int newWidth = ((int)(texture->width * scale + 3) / 8) * 8;
		int newHeight = ((int)(texture->height * scale + 3) / 8) * 8;

		resizeTexture(texture->name, newWidth, newHeight);
	}
}

bool Model::port_to_hl() {
	int modelType = get_model_type();

	if (hasExternalTextures())
		mergeExternalTextures(false);
	if (hasExternalSequences())
		mergeExternalSequences(false);

	if (modelType == PMODEL_SVEN_COOP_5 || modelType == PMODEL_SVEN_COOP_4 || modelType == PMODEL_SVEN_COOP_3) {
		// Sven Co-op 3.0 is the first version to break HL compatibility by removing and reordering
		// animations. All versions after that appended more animations which HL doesn't use. Separate
		// porting logic is needed for converting between SC versions.
		if (modelType != PMODEL_SVEN_COOP_5) {
			printf("Warning: This model is missing animations which will not be automatically added.\n");
		}
		
		port_sc_animations_to_hl();
	}
	else {
		printf("Don't know how to port this model.\n");
		return false;
	}

	hackshade();
	downscale_textures(0x80000);
	int eventsEdited = wavify();

	if (eventsEdited) {
		printf("Applied wav extension to %d audio events\n", eventsEdited);
	}

	//printModelDataOrder();

	return validate();
}

int Model::get_model_type() {
	if (isExtModel()) {
		cout << "Model type: External textures/animations\n";
		return PMODEL_EXTERNAL;
	}

	int biggestTextureSize = 0;
	for (int i = 0; i < header->numtextures; i++) {
		data.seek(header->textureindex + i * sizeof(mstudiotexture_t));
		mstudiotexture_t* texture = (mstudiotexture_t*)data.get();
		int sz = texture->width * texture->height;;
		if (sz > biggestTextureSize) {
			biggestTextureSize = sz;
		}
	}

	for (int k = 0; k < header->numseq; k++) {
		data.seek(header->seqindex + k * sizeof(mstudioseqdesc_t));
		mstudioseqdesc_t* seq = (mstudioseqdesc_t*)data.get();

		for (int i = 0; i < g_modelTypes.size(); i++) {
			ModelType& mtype = g_modelTypes[i];

			if (biggestTextureSize > mtype.max_texture_size)
				continue;

			if (k < mtype.anims.size()) {
				if (mtype.anims[k].activity == seq->activity) {
					mtype.num_match_act++;
				}
				if (string(mtype.anims[k].name) == toLowerCase(seq->label)) {
					mtype.num_match_name++;
				}
			}
		}

		//printf("{%d, \"%s\"},\n", seq->activity, seq->label);
	}
	//printf("\n");

	// match by most animations first (least likely to match = more certainty)
	std::sort(g_modelTypes.begin(), g_modelTypes.end(), [](const ModelType& a, const ModelType& b) {
		return a.anims.size() > b.anims.size();
	});

	float bestMatchPer = 0;
	float secondBestMatchPer = 0;
	int bestMatch = -1;

	for (int i = 0; i < g_modelTypes.size(); i++) {
		ModelType& mtype = g_modelTypes[i];

		// not comparing names because some modelers write their names in animation labels
		if (mtype.num_match_act == mtype.anims.size()) {
			cout << "Model type: " << mtype.modname << " (100% match)\n";
			return mtype.modcode;
		}
		
		float matchPer = (mtype.num_match_act + mtype.num_match_name) / (float)(mtype.anims.size() * 2);
		mtype.match_percent = matchPer;

		if (matchPer >= bestMatchPer) {
			secondBestMatchPer = bestMatchPer;

			bestMatchPer = matchPer;
			bestMatch = i;
		}
		else if (matchPer > secondBestMatchPer) {
			secondBestMatchPer = matchPer;
		}
	}

	if ((bestMatchPer > 0.95f && secondBestMatchPer < 0.85f) || (bestMatchPer > 0.90f && secondBestMatchPer < 0.75f)) {
		ModelType& mtype = g_modelTypes[bestMatch];
		cout << "Model type: " << mtype.modname << " (" << (int)(bestMatchPer * 100) << "% match)\n";
		return mtype.modcode;
	}

	// no exact matches, find fewest errors when considering animation names
	std::sort(g_modelTypes.begin(), g_modelTypes.end(), [](const ModelType& a, const ModelType& b) {
		return a.match_percent > b.match_percent;
	});

	for (int i = 0; i < g_modelTypes.size(); i++) {
		ModelType& mtype = g_modelTypes[i];
		if (mtype.match_percent > 0)
			printf("%-22s = %3d / %-3d  match (%d%%)\n", mtype.modname,
				mtype.num_match_act + mtype.num_match_name, mtype.anims.size()*2,
				(int)(mtype.match_percent * 100));
	}

	cout << "Model type: Unknown\n";
	return PMODEL_UNKNOWN;
}

int Model::get_animation_size(int sequence) {
	data.seek(header->seqindex + sequence * sizeof(mstudioseqdesc_t));
	mstudioseqdesc_t* seq = (mstudioseqdesc_t*)data.get();

	data.seek(header->seqgroupindex);
	mstudioseqgroup_t* pseqgroup = (mstudioseqgroup_t*)data.get();
	data.seek(pseqgroup->data + seq->animindex);
	mstudioanim_t* panim = (mstudioanim_t*)data.get();
	int animDataSz = 0;

	for (int b = 0; b < seq->numblends; b++) {
		for (int i = 0; i < header->numbones; i++, panim++) {
			animDataSz += sizeof(mstudioanim_t);

			for (int j = 0; j < 6; j++) {
				if (panim->offset[j] == 0) {
					continue; // no data
				}

				mstudioanimvalue_t* pvaluehdr = (mstudioanimvalue_t*)((uint8_t*)panim + panim->offset[j]);
				animDataSz += (pvaluehdr->num.valid + 1) * sizeof(mstudioanimvalue_t);

				int frameCount = pvaluehdr->num.total;
				while (frameCount < seq->numframes) {
					pvaluehdr += pvaluehdr->num.valid + 1;
					frameCount += pvaluehdr->num.total;
					animDataSz += (pvaluehdr->num.valid + 1) * sizeof(mstudioanimvalue_t);
				}
			}
		}
	}

	return animDataSz;
}

vector<short> Model::get_animation_data(int sequence) {
	vector<short> values;

	data.seek(header->seqindex + sequence * sizeof(mstudioseqdesc_t));
	mstudioseqdesc_t* seq = (mstudioseqdesc_t*)data.get();

	if (seq->seqgroup > 0) {
		printf("External sequences not supported\n");
		return values;
	}

	mstudioanim_t* panim = getAnimFrames(sequence);

	for (int b = 0; b < seq->numblends; b++) {
		for (int i = 0; i < header->numbones; i++, panim++) {
			for (int j = 0; j < 6; j++) {
				if (panim->offset[j] == 0) {
					continue; // no data
				}

				mstudioanimvalue_t* pvaluehdr = (mstudioanimvalue_t*)((uint8_t*)panim + panim->offset[j]);

				int frameCount = 0;
				do {
					for (int k = 0; k < pvaluehdr->num.total && frameCount < seq->numframes; k++) {
						int valIdx = min(k, pvaluehdr->num.valid);
						values.push_back(*((short*)pvaluehdr + 1 + valIdx));
						frameCount++;
					}

					pvaluehdr += pvaluehdr->num.valid + 1; // 1 = skip the header too
				} while (frameCount < seq->numframes);
			}
		}
	}

	return values;
}

void Model::optimize() {
	data.seek(header->seqindex);
	mstudioseqdesc_t* seqs = (mstudioseqdesc_t*)data.get();

	vector<vector<short>> animdata;
	for (int k = 0; k < header->numseq; k++) {
		animdata.push_back(get_animation_data(k));
	}

	unordered_map<int, int> dedups;
	int bytesSaved = 0;

	for (int i = 0; i < header->numseq; i++) {
		for (int k = 0; k < header->numseq && k < i; k++) {
			if (i == k)
				continue;
			if (animdata[i].size() > animdata[k].size())
				continue;
			if (seqs[i].animindex == seqs[k].animindex)
				continue;
			if (memcmp(&animdata[i][0], &animdata[k][0], animdata[i].size() * sizeof(short)))
				continue;

			dedups[i] = k;

			if (animdata[i].size() < animdata[k].size()) {
				printf("Animation %d is a sub animation of %d\n", i, k);
			}
			else {
				printf("Animation %d is a duplicate of %d\n", i, k);
			}
		}
	}

	for (auto item : dedups) {
		int dst = item.first;
		int src = item.second;

		printf("Deduplicating anim %d\n", dst);

		int animOffset = seqs[dst].animindex;
		int animSz = get_animation_size(dst);
		animSz = animSz - (animSz & 3);
		bytesSaved += animSz;

		data.seek(animOffset);
		removeData(animSz);
		updateIndexes(animOffset, -animSz);

		data.seek(header->seqindex);
		seqs = (mstudioseqdesc_t*)data.get();
		seqs[dst].animindex = seqs[src].animindex;
		seqs[dst].numblends = seqs[src].numblends;
	}

	printModelDataOrder();

	printf("Removed %d bytes\n", bytesSaved);
}