#include "MdlRenderer.h"
#include "util.h"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <ShaderProgram.h>
#include "mat4x4.h"
#include <float.h>
#include "Renderer.h"
#include <cstring>

void glCheckError(const char* checkMessage);

MdlRenderer::MdlRenderer(ShaderProgram* shader, bool legacy_mode, string modelPath) {
	this->fpath = modelPath;
	this->legacy_mode = legacy_mode;
	this->shader = shader;
	valid = false;
	needTransform = true;
	u_boneTexture = -1;
	oldLegacyMode = false;

	loadData();
	upload();
}

MdlRenderer::~MdlRenderer() {
	if (valid) {
		for (int i = 0; i < numTextures; i++) {
			delete glTextures[i];
		}
		delete[] glTextures;

		for (int b = 0; b < header->numbodyparts; b++) {
			data.seek(header->bodypartindex + b * sizeof(mstudiobodyparts_t));
			mstudiobodyparts_t* bod = (mstudiobodyparts_t*)data.get();

			for (int m = 0; m < bod->nummodels; m++) {
				data.seek(bod->modelindex + m * sizeof(mstudiomodel_t));
				mstudiomodel_t* mod = (mstudiomodel_t*)data.get();

				for (int k = 0; k < mod->nummesh; k++) {
					MdlMeshRender& render = meshBuffers[b][m][k];

					delete[] render.origVerts;
					delete[] render.origNorms;
					delete[] render.transformVerts;
					delete[] render.verts;
					delete render.buffer;
				}

				delete[] meshBuffers[b][m];
			}

			delete[] meshBuffers[b];
		}
		for (int i = 0; i < MAXSTUDIOSEQUENCES; i++) {
			if (seqheaders[i].getBuffer())
				seqheaders[i].freeBuf();
		}
	}

	if (header != texheader) {
		delete[] texdata.getBuffer();
		texheader = NULL;
	}

	delete[] data.getBuffer();
	header = NULL;

	if (u_boneTexture != -1)
		glDeleteTextures(1, &u_boneTexture);
}

bool MdlRenderer::validate() {
	if (string(header->name).length() <= 0) {
		return false;
	}

	if (header->id != 1414743113) {
		printf("ERROR: Invalid ID in model header\n");
		return false;
	}

	if (header->version != 10) {
		printf("ERROR: Invalid version in model header\n");
		return false;
	}

	if (header->numseqgroups >= 10000) {
		printf("ERROR: Too many seqgroups (%d) in model\n", header->numseqgroups);
		return false;
	}

	if (data.eom())
		return false;

	for (int b = 0; b < header->numbodyparts; b++) {
		// Try loading required model info
		data.seek(header->bodypartindex + b*sizeof(mstudiobodyparts_t));
		mstudiobodyparts_t* bod = (mstudiobodyparts_t*)data.get();

		for (int i = 0; i < bod->nummodels; i++) {
			data.seek(bod->modelindex + i * sizeof(mstudiomodel_t));
			mstudiomodel_t* mod = (mstudiomodel_t*)data.get();

			if (data.eom()) {
				printf("ERROR: Failed to load body %d / %d\n", i, bod->nummodels);
				return false;
			}

			for (int k = 0; k < mod->nummesh; k++) {
				data.seek(mod->meshindex + k * sizeof(mstudiomesh_t));

				if (data.eom()) {
					printf("ERROR: Failed to load mesh %d in model %d\n", k, i);
					return false;
				}

				mstudiomesh_t* mesh = (mstudiomesh_t*)data.get();

				data.seek(mesh->normindex + (mesh->numnorms * sizeof(vec3)) - 1);
				if (data.eom()) {
					printf("ERROR: Failed to load normals for mesh %d in model %d\n", k, i);
					return false;
				}

				data.seek(mesh->triindex + (mesh->numtris * sizeof(mstudiotrivert_t) + 2) - 1);
				if (data.eom()) {
					printf("ERROR: Failed to load triangles for mesh %d in model %d\n", k, i);
					return false;
				}

				
				data.seek(mesh->triindex);
				short* ptricmds = (short*)data.get();
				short* oldcmd = ptricmds;

				int p;
				int maxVertIdx = 0;
				int maxNormIdx = 0;
				while (p = *(ptricmds++))
				{
					if (p < 0) {
						p = -p;
					}
					for (; p > 0; p--, ptricmds += 4) {
						data.seek(mesh->triindex + (ptricmds - oldcmd));
						if (data.eom()) {
							printf("ERROR: Bad tri list in mesh %d in model %d\n", k, i);
							return false;
						}

						maxVertIdx = max(maxVertIdx, (int)ptricmds[0]);
						maxNormIdx = max(maxNormIdx, (int)ptricmds[1]);
					}
				}

				data.seek(mesh->normindex + maxNormIdx*sizeof(vec3));
				if (data.eom()) {
					printf("ERROR: Bad tri norm idx in mesh %d in model %d\n", k, i);
					return false;
				}

				data.seek(mod->vertindex + maxVertIdx * sizeof(vec3));
				if (data.eom()) {
					printf("ERROR: Bad tri vert idx in mesh %d in model %d\n", k, i);
					return false;
				}
			}
		}
	}

	

	for (int i = 0; i < header->numseq; i++) {
		data.seek(header->seqindex + i * sizeof(mstudioseqdesc_t));
		if (data.eom()) {
			printf("ERROR: Failed to load sequence %d / %d\n", i, header->numseq);
			return false;
		}

		mstudioseqdesc_t* seq = (mstudioseqdesc_t*)data.get();

		for (int k = 0; k < seq->numevents; k++) {
			data.seek(seq->eventindex + k * sizeof(mstudioevent_t));

			if (data.eom()) {
				printf("ERROR: Failed to load event %d / %d in sequence %d\n", k, seq->numevents, i);
				return false;
			}

			mstudioevent_t* evt = (mstudioevent_t*)data.get();
		}

		data.seek(seq->animindex + (seq->numblends * header->numbones * sizeof(mstudioanim_t) * 6) - 1);
		if (data.eom()) {
			printf("ERROR: Failed to load bone data for sequence %d / %d\n", i, header->numseq);
			return false;
		}
	}

	for (int i = 0; i < header->numbones; i++) {
		data.seek(header->boneindex + i * sizeof(mstudiobone_t));
		if (data.eom()) {
			printf("ERROR: Failed to load sequence %d / %d\n", i, header->numseq);
			return false;
		}

		mstudiobone_t* bone = (mstudiobone_t*)data.get();
		if (bone->parent < -1 || bone->parent >= header->numbones) {
			printf("ERROR: Bone %d has invalid parent %d\n", i, bone->parent);
			return false;
		}
	}

	for (int i = 0; i < header->numbonecontrollers; i++) {
		data.seek(header->bonecontrollerindex + i * sizeof(mstudiobonecontroller_t));
		if (data.eom()) {
			printf("ERROR: Failed to load bone controller %d / %d\n", i, header->numbonecontrollers);
			return false;
		}

		mstudiobonecontroller_t* ctl = (mstudiobonecontroller_t*)data.get();
		if (ctl->bone < -1 || ctl->bone >= header->numbones) {
			printf("ERROR: Controller %d references invalid bone \n", i, ctl->bone);
			return false;
		}
	}

	for (int i = 0; i < header->numhitboxes; i++) {
		data.seek(header->hitboxindex + i * sizeof(mstudiobbox_t));
		if (data.eom()) {
			printf("ERROR: Failed to load bone controller %d / %d\n", i, header->numhitboxes);
			return false;
		}

		mstudiobbox_t* box = (mstudiobbox_t*)data.get();
		if (box->bone < -1 || box->bone >= header->numbones) {
			printf("ERROR: Hitbox %d references invalid bone %d\n", i, box->bone);
			return false;
		}
	}

	for (int i = 0; i < header->numseqgroups; i++) {
		data.seek(header->seqgroupindex + i * sizeof(mstudioseqgroup_t));
		if (data.eom()) {
			printf("ERROR: Failed to load sequence group %d/%d\n", i, header->numseqgroups);
			return false;
		}

		mstudioseqgroup_t* grp = (mstudioseqgroup_t*)data.get();
	}

	for (int i = 0; i < header->numtextures; i++) {
		data.seek(header->textureindex + i * sizeof(mstudiotexture_t));
		if (data.eom()) {
			printf("ERROR: Failed to load texture %d/%d\n", i, header->numtextures);
			return false;
		}

		mstudiotexture_t* tex = (mstudiotexture_t*)data.get();
		data.seek(tex->index + (tex->width * tex->height + 256 * 3) - 1);
		if (data.eom()) {
			printf("ERROR: Failed to load texture data %d/%d\n", i, header->numtextures);
			return false;
		}
	}

	for (int i = 0; i < header->numskinfamilies; i++) {
		data.seek(header->skinindex + i * sizeof(short) * header->numskinref);
		if (data.eom()) {
			printf("ERROR: Failed to load skin family %d/%d\n", i, header->numskinfamilies);
			return false;
		}
	}

	for (int i = 0; i < header->numattachments; i++) {
		data.seek(header->attachmentindex + i * sizeof(mstudioattachment_t));
		if (data.eom()) {
			printf("ERROR: Failed to load attachment %d/%d\n", i, header->numattachments);
			return false;
		}

		mstudioattachment_t* att = (mstudioattachment_t*)data.get();
		if (att->bone < -1 || att->bone >= header->numbones) {
			printf("ERROR: Attachment %d references invalid bone %d\n", i, att->bone);
			return false;
		}
	}

	return true;
}

bool MdlRenderer::isEmpty() {
	bool isEmptyModel = true;

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

bool MdlRenderer::hasExternalTextures() {
	// textures aren't needed if the model has no triangles
	return header->numtextures == 0 && !isEmpty();
}

bool MdlRenderer::hasExternalSequences() {
	return header->numseqgroups > 1;
}

void MdlRenderer::loadData() {
	int len;
	char* buffer = loadFile(fpath, len);
	if (!buffer) {
		return;
	}

	data = mstream(buffer, len);
	texheader = header = (studiohdr_t*)buffer;
	texdata = mstream(buffer, len);
	if (!validate() || isEmpty()) {
		return;
	}

	memset(iController, 127, 4);
	memset(iBlender, 0, 2);
	memset(cachedBounds, 0, sizeof(cachedBounds));
	iMouth = 0;

	if (!loadTextureData() || !loadSequenceData()) {
		return;
	}

	vec3 angles;
	SetUpBones(angles, 0, 0);
	loadMeshes();
	//transformVerts();

	// precalculate anim bounds
	for (int i = 0; i < header->numseq; i++) {
		vec3 mins, maxs;
		getModelBoundingBox(vec3(), i, mins, maxs);
	}

	valid = true;
}

void MdlRenderer::upload() {
	for (int i = 0; i < numTextures; i++) {
		if (!glTextures[i]->uploaded) {
			glTextures[i]->upload(glTextures[i]->format);
		}
	}

	glCheckError("MDL texture uploads");

	for (int b = 0; b < header->numbodyparts; b++) {
		// Try loading required model info
		data.seek(header->bodypartindex + b * sizeof(mstudiobodyparts_t));
		mstudiobodyparts_t* bod = (mstudiobodyparts_t*)data.get();

		for (int m = 0; m < bod->nummodels; m++) {
			data.seek(bod->modelindex + m * sizeof(mstudiomodel_t));
			mstudiomodel_t* mod = (mstudiomodel_t*)data.get();

			for (int k = 0; k < mod->nummesh; k++) {
				if (!meshBuffers[b][m][k].buffer->isUploaded()) {
					meshBuffers[b][m][k].buffer->upload();
				}
			}
		}
	}

	glCheckError("MDL body mesh uploads");

	shader->bind();

	glGenTextures(1, &u_boneTexture);
	glBindTexture(GL_TEXTURE_2D, u_boneTexture);

	// disable filtering and mipmaps so the texture can be used as a lookup table
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);

	// allocate data so subImage can be used for faster updates
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, 4, MAXSTUDIOBONES, 0, GL_RGBA, GL_FLOAT, m_bonetransform);

	glCheckError("MDL bone texture creation");
}

bool MdlRenderer::loadTextureData() {
	bool externalTextures = hasExternalTextures();

	if (externalTextures) {
		int lastDot = fpath.find_last_of(".");
		if (lastDot == -1) {
			printf("Failed to load external texture model for: %s\n", fpath.c_str());
			return false;
		}

		string ext = fpath.substr(lastDot);
		string basepath = fpath.substr(0, lastDot);
		string tpath = basepath + "t" + ext;

		int len;
		char* buffer = loadFile(tpath, len);
		if (!buffer) {
			printf("Failed to load external texture model: %s\n", tpath.c_str());
			return false;
		}

		texdata = mstream(buffer, len);
		texheader = (studiohdr_t*)texdata.getBuffer();
	}

	numTextures = texheader->numtextures;

	glTextures = new Texture*[texheader->numtextures];
	memset(glTextures, 0, sizeof(Texture*)* texheader->numtextures);

	for (int i = 0; i < texheader->numtextures; i++) {
		texdata.seek(texheader->textureindex + i * sizeof(mstudiotexture_t));
		if (texdata.eom()) {
			printf("ERROR: Failed to load texture %d/%d\n", i, texheader->numtextures);
			continue;
		}

		mstudiotexture_t* tex = (mstudiotexture_t*)texdata.get();
		texdata.seek(tex->index + (tex->width * tex->height + 256 * 3) - 1);
		if (texdata.eom()) {
			printf("ERROR: Failed to load texture %d/%d\n", i, texheader->numtextures);
			continue;
		}

		texdata.seek(tex->index);
		uint8_t* srcData = (uint8_t*)texdata.get();
		int imageDataSz = tex->width * tex->height;

		texdata.seek(tex->index + imageDataSz);
		COLOR3* palette = (COLOR3*)texdata.get();

		if (tex->flags & STUDIO_NF_MASKED) {
			COLOR4* imageData = new COLOR4[imageDataSz];

			for (int k = 0; k < imageDataSz; k++) {
				if (srcData[k] == 255) {
					imageData[k] = COLOR4(0, 0, 0, 0);
				}
				else {
					imageData[k] = COLOR4(palette[srcData[k]], 255);
				}
			}

			glTextures[i] = new Texture(tex->width, tex->height, imageData);
			glTextures[i]->format = GL_RGBA;
			//glTextures[i]->generateMipMaps(3);
		} else{
			COLOR3* imageData = new COLOR3[imageDataSz];

			for (int k = 0; k < imageDataSz; k++) {
				imageData[k] = palette[srcData[k]];
			}

			glTextures[i] = new Texture(tex->width, tex->height, imageData);
			glTextures[i]->format = GL_RGB;
			//glTextures[i]->generateMipMaps(3);
		}
	}

	return true;
}

bool MdlRenderer::loadSequenceData() {
	if (!hasExternalSequences()) {
		return true;
	}

	int lastDot = fpath.find_last_of(".");
	string ext = fpath.substr(lastDot);
	string basepath = fpath.substr(0, lastDot);

	for (int i = 1; i < header->numseqgroups && i < MAXSTUDIOSEQUENCES; i++) {
		string suffix = i < 10 ? "0" + to_string(i) : to_string(i);
		string spath = basepath + suffix + ext;

		if (!fileExists(spath)) {
			printf("External sequence model not found: %s\n", spath.c_str());
			return false;
		}

		int len;
		char* buffer = loadFile(spath, len);
		if (!buffer) {
			printf("Failed to load external texture model: %s\n", spath.c_str());
			return false;
		}

		seqheaders.push_back(mstream(buffer, len));
	}

	return true;
}

bool MdlRenderer::loadMeshes() {
	int uiDrawnPolys = 0;
	int meshBytes = 0;

	meshBuffers = new MdlMeshRender * *[header->numbodyparts];
	memset(meshBuffers, 0, sizeof(MdlMeshRender**) * header->numbodyparts);

	for (int b = 0; b < header->numbodyparts; b++) {
		// Try loading required model info
		data.seek(header->bodypartindex + b * sizeof(mstudiobodyparts_t));
		mstudiobodyparts_t* bod = (mstudiobodyparts_t*)data.get();

		meshBuffers[b] = new MdlMeshRender * [bod->nummodels];
		memset(meshBuffers[b], 0, sizeof(MdlMeshRender*) * bod->nummodels);

		for (int m = 0; m < bod->nummodels; m++) {
			data.seek(bod->modelindex + m * sizeof(mstudiomodel_t));
			mstudiomodel_t* mod = (mstudiomodel_t*)data.get();

			meshBuffers[b][m] = new MdlMeshRender[mod->nummesh];
			memset(meshBuffers[b][m], 0, sizeof(MdlMeshRender) * mod->nummesh);

			data.seek(mod->vertindex);
			vec3* pstudioverts = (vec3*)data.get();

			data.seek(mod->normindex);
			vec3* pstudionorms = (vec3*)data.get();

			data.seek(mod->vertinfoindex);
			uint8_t* pvertbone = (uint8_t*)data.get();

			for (int k = 0; k < mod->nummesh; k++) {
				data.seek(mod->meshindex + k * sizeof(mstudiomesh_t));

				if (data.eom()) {
					printf("ERROR: Failed to load mesh %d in model %d\n", k, m);
					continue;
				}

				mstudiomesh_t* mesh = (mstudiomesh_t*)data.get();

				data.seek(mesh->normindex + (mesh->numnorms * sizeof(vec3)) - 1);
				if (data.eom()) {
					printf("ERROR: Failed to load normals for mesh %d in model %d\n", k, m);
					continue;
				}

				data.seek(mesh->triindex + (mesh->numtris * sizeof(mstudiotrivert_t) + 2) - 1);
				if (data.eom()) {
					printf("ERROR: Failed to load triangles for mesh %d in model %d\n", k, m);
					continue;
				}

				texdata.seek(texheader->skinindex);
				short* skins = (short*)texdata.get();
				int texId = skins[mesh->skinref];

				if (texId < 0 || texId >= numTextures) {
					printf("ERROR: invalid texture ref %d (max %d)\n", texId, header->numtextures);
					continue;
				}

				texdata.seek(texheader->textureindex + texId * sizeof(mstudiotexture_t));
				mstudiotexture_t* texture = (mstudiotexture_t*)texdata.get();
				meshBuffers[b][m][k].skinref = mesh->skinref;

				data.seek(mesh->triindex);
				short* ptricmds = (short*)data.get();

				int totalElements = 0;
				int texCoordIdx = 0;
				int colorIdx = 0;
				int vertexIdx = 0;
				int stripIdx = 0;
				int origVertIdx = 0;
				int origNormIdx = 0;
				int debugIdx = 0;

				const float s = 1.0 / (float)texture->width;
				const float t = 1.0 / (float)texture->height;

				vector<MdlVert> mdlVerts;

				mdlVerts.reserve(MAXSTUDIOVERTS * 3);
				int p;

				while (p = *(ptricmds++))
				{
					int drawMode = GL_TRIANGLE_STRIP;
					if (p < 0)
					{
						p = -p;
						drawMode = GL_TRIANGLE_FAN;
					}

					int polies = p - 2;
					uiDrawnPolys += polies;

					int elementsThisStrip = 0;
					int fanStartIdx = totalElements;

					for (; p > 0; p--, ptricmds += 4)
					{
						if (elementsThisStrip++ >= 3) { // first 3 verts are always the first triangle
							// convert to GL_TRIANGLES
							if (drawMode == GL_TRIANGLE_STRIP) {
								mdlVerts.push_back(mdlVerts[totalElements - 2]);
								mdlVerts.push_back(mdlVerts[totalElements - 1]);

								totalElements += 2;
								elementsThisStrip += 2;
							}
							else if (drawMode == GL_TRIANGLE_FAN) {
								mdlVerts.push_back(mdlVerts[fanStartIdx]);
								mdlVerts.push_back(mdlVerts[totalElements - 1]);

								totalElements += 2;
								elementsThisStrip += 2;
							}
						}

						MdlVert vert;

						if (texture->flags & STUDIO_NF_ADDITIVE) {
							vert.color = vec4(1.0f, 1.0f, 1.0f, 1.0f);
						}
						else {
							vert.color = vec4(pstudionorms[ptricmds[1]], 0);
						}

						if (texture->flags & STUDIO_NF_CHROME) {
							// real UVs calculated in shader
							vert.uv = vec2(0.5f, 0.5f);
						}
						else {
							vert.uv = vec2(ptricmds[2] * s, ptricmds[3] * t);
						}

						// TODO: hmmm
						//vert.color.w = m_pRenderInfo->flTransparency;

						vert.pos = pstudioverts[ptricmds[0]];
						vert.origVert = ptricmds[0];
						vert.origNorm = ptricmds[1];
						mdlVerts.push_back(vert);

						totalElements++;
					}

					// flip odd tris in strips (simpler than adding special logic when appending verts)
					if (drawMode == GL_TRIANGLE_STRIP) {
						for (int p = 1; p < polies; p += 2) {
							int polyOffset = p * 3;

							int vstart = polyOffset + fanStartIdx + 1;
							MdlVert t = mdlVerts[vstart];
							mdlVerts[vstart] = mdlVerts[vstart + 1];
							mdlVerts[vstart + 1] = t;
						}
					}
				}

				//debugf("%d %d %d - %d polys, %d verts, %d render verts\n", b, m, k, totalElements / 3, mod->numverts, totalElements);

				MdlMeshRender& render = meshBuffers[b][m][k];

				render.origVerts = new short[totalElements];
				render.origNorms = new short[totalElements];
				render.transformVerts = new vec3[totalElements];

				vector<boneVert> allVerts;
				allVerts.reserve(totalElements);

				for (int i = 0; i < totalElements; i++) {
					MdlVert& v = mdlVerts[i];
					render.origVerts[i] = v.origVert;
					render.transformVerts[i] = v.pos;
					render.origNorms[i] = v.origNorm;

					boneVert bvert;
					bvert.pos = v.pos;
					bvert.normal = v.color.xyz();
					bvert.uv = v.uv;
					bvert.bone = pvertbone[v.origVert] + 0.1f;

					allVerts.push_back(bvert);
				}

				meshBuffers[b][m][k].flags = texture->flags;
				meshBuffers[b][m][k].verts = new boneVert[totalElements];
				meshBuffers[b][m][k].numVerts = totalElements;
				memcpy(meshBuffers[b][m][k].verts, &allVerts[0], totalElements * sizeof(boneVert));
				meshBuffers[b][m][k].buffer = new VertexBuffer(shader, NORM_3F | TEX_2F | POS_3F, meshBuffers[b][m][k].verts, meshBuffers[b][m][k].numVerts);
				meshBuffers[b][m][k].buffer->addAttribute(1, GL_FLOAT, GL_FALSE, "vBone");
				//meshBuffers[b][m][k].buffer->upload();

				meshBytes += totalElements * (sizeof(uint16_t) + sizeof(uint16_t) + sizeof(boneVert));
			}
		}
	}

	//debugf("Total polys: %d, Mesh kb: %d\n", uiDrawnPolys, (int)(meshBytes / 1024.0f));

	return true;
}

mstudioanim_t* MdlRenderer::GetAnim(mstudioseqdesc_t* pseqdesc) {
	data.seek(header->seqgroupindex + pseqdesc->seqgroup*sizeof(mstudioseqgroup_t));
	mstudioseqgroup_t* pseqgroup = (mstudioseqgroup_t*)data.get();

	data.seek(pseqgroup->data + pseqdesc->animindex);
	mstudioanim_t* anim = (mstudioanim_t*)data.get();
	int externalIdx = pseqdesc->seqgroup - 1;

	if (externalIdx < 0) {
		return anim;
	}
	else if (externalIdx >= seqheaders.size()) {
		printf("Invalid sequence group %d\n", pseqdesc->seqgroup);
		return anim;
	}
	
	mstream extdat = seqheaders[externalIdx];

	if (!extdat.getBuffer()) {
		printf("Sequence group %d is invalid or not loaded\n", pseqdesc->seqgroup);
		return anim;
	}

	extdat.seek(pseqgroup->data + pseqdesc->animindex);
	return extdat.eom() ? anim : (mstudioanim_t*)extdat.get();
}

mstudioseqdesc_t* MdlRenderer::getSequence(int seq) {
	if (seq < 0 || seq > header->numseq) {
		return NULL;
	}

	data.seek(header->seqindex + seq * sizeof(mstudioseqdesc_t));
	return (mstudioseqdesc_t*)data.get();
}

void MdlRenderer::CalcBoneAdj()
{
	data.seek(header->bonecontrollerindex);
	mstudiobonecontroller_t* pbonecontroller = (mstudiobonecontroller_t*) data.get();

	for (int j = 0; j < header->numbonecontrollers; j++)
	{
		const auto i = pbonecontroller[j].index;

		float value;

		if (i <= 3)
		{
			// check for 360% wrapping
			if (pbonecontroller[j].type & STUDIO_RLOOP)
			{
				value = iController[i] * (360.0 / 256.0) + pbonecontroller[j].start;
			}
			else
			{
				value = iController[i] / 255.0;
				if (value < 0) value = 0;
				if (value > 1.0) value = 1.0;
				value = (1.0 - value) * pbonecontroller[j].start + value * pbonecontroller[j].end;
			}
			// Con_DPrintf( "%d %d %f : %f\n", m_controller[j], m_prevcontroller[j], value, dadt );
		}
		else
		{
			value = iMouth / 64.0;
			if (value > 1.0) value = 1.0;
			value = (1.0 - value) * pbonecontroller[j].start + value * pbonecontroller[j].end;
			// Con_DPrintf("%d %f\n", mouthopen, value );
		}
		switch (pbonecontroller[j].type & STUDIO_TYPES)
		{
		case STUDIO_XR:
		case STUDIO_YR:
		case STUDIO_ZR:
			m_Adj[j] = value * (PI / 180.0);
			break;
		case STUDIO_X:
		case STUDIO_Y:
		case STUDIO_Z:
			m_Adj[j] = value;
			break;
		}
	}
}

void AngleQuaternion(const vec3& angles, vec4& quaternion)
{
	float		angle;
	float		sr, sp, sy, cr, cp, cy;

	// FIXME: rescale the inputs to 1/2 angle
	angle = angles.z * 0.5;
	sy = sin(angle);
	cy = cos(angle);
	angle = angles.y * 0.5;
	sp = sin(angle);
	cp = cos(angle);
	angle = angles.x * 0.5;
	sr = sin(angle);
	cr = cos(angle);

	quaternion.x = sr * cp * cy - cr * sp * sy;
	quaternion.y = cr * sp * cy + sr * cp * sy;
	quaternion.z = cr * cp * sy - sr * sp * cy;
	quaternion.w = cr * cp * cy + sr * sp * sy;
}

void QuaternionSlerp(const vec4& pVec, vec4& qVec, float t, vec4& qtVec)
{
	int i;
	float omega, cosom, sinom, sclp, sclq;
	float* p = (float*)&pVec;
	float* q = (float*)&qVec;
	float* qt = (float*)&qtVec;

	// decide if one of the quaternions is backwards
	float a = 0;
	float b = 0;
	for (i = 0; i < 4; i++) {
		a += (p[i] - q[i]) * (p[i] - q[i]);
		b += (p[i] + q[i]) * (p[i] + q[i]);
	}
	if (a > b) {
		for (i = 0; i < 4; i++) {
			q[i] = -q[i];
		}
	}

	cosom = p[0] * q[0] + p[1] * q[1] + p[2] * q[2] + p[3] * q[3];

	if ((1.0 + cosom) > 0.00000001) {
		if ((1.0 - cosom) > 0.00000001) {
			omega = acos(cosom);
			sinom = sin(omega);
			sclp = sin((1.0 - t) * omega) / sinom;
			sclq = sin(t * omega) / sinom;
		}
		else {
			sclp = 1.0 - t;
			sclq = t;
		}
		for (i = 0; i < 4; i++) {
			qt[i] = sclp * p[i] + sclq * q[i];
		}
	}
	else {
		qt[0] = -p[1];
		qt[1] = p[0];
		qt[2] = -p[3];
		qt[3] = p[2];
		sclp = sin((1.0 - t) * 0.5 * PI);
		sclq = sin(t * 0.5 * PI);
		for (i = 0; i < 3; i++) {
			qt[i] = sclp * p[i] + sclq * qt[i];
		}
	}
}

bool VectorCompare(const vec3& lhs, const vec3& rhs) {
	if (fabs(lhs.x - rhs.x) > EQUAL_EPSILON
		|| fabs(lhs.y - rhs.y) > EQUAL_EPSILON
		|| fabs(lhs.z - rhs.z) > EQUAL_EPSILON) {
		return false;
	}

	return true;
}

void VectorTransform(const vec3& in1, float in2[3][4], vec3& out) {
	// convert coordinate system
	out.x = dotProduct(in1, *(vec3*)(&in2[0][0])) + in2[0][3];
	out.z = -(dotProduct(in1, *(vec3*)(&in2[1][0])) + in2[1][3]);
	out.y = dotProduct(in1, *(vec3*)(&in2[2][0])) + in2[2][3];
}

void VectorRotate(const vec3& in1, float in2[3][4], vec3& out) {
	// convert coordinate system
	out.x = dotProduct(in1, *(vec3*)(&in2[0][0]));
	out.y = dotProduct(in1, *(vec3*)(&in2[1][0]));
	out.z = dotProduct(in1, *(vec3*)(&in2[2][0]));
}

void VectorIRotate(vec3& vector, float matrix[3][4], vec3& outResult)
{
	outResult.x = vector.x * matrix[0][0] + vector.y * matrix[1][0] + vector.z * matrix[2][0];
	outResult.y = vector.x * matrix[0][1] + vector.y * matrix[1][1] + vector.z * matrix[2][1];
	outResult.z = vector.x * matrix[0][2] + vector.y * matrix[1][2] + vector.z * matrix[2][2];
}

void VectorIRotate(vec3& vector, float** matrix, vec3& outResult)
{
	outResult.x = vector.x * matrix[0][0] + vector.y * matrix[1][0] + vector.z * matrix[2][0];
	outResult.y = vector.x * matrix[0][1] + vector.y * matrix[1][1] + vector.z * matrix[2][1];
	outResult.z = vector.x * matrix[0][2] + vector.y * matrix[1][2] + vector.z * matrix[2][2];
}

void MdlRenderer::CalcBoneQuaternion(const int frame, const float s, const mstudiobone_t* const pbone, const mstudioanim_t* const panim, vec4& q)
{
	vec3 angle1Vec;
	vec3 angle2Vec;
	float* angle1 = (float*)&angle1Vec;
	float* angle2 = (float*)&angle2Vec;

	for (int j = 0; j < 3; j++)
	{
		if (panim->offset[j + 3] == 0)
		{
			angle2[j] = angle1[j] = pbone->value[j + 3]; // default;
		}
		else
		{
			mstudioanimvalue_t* panimvalue = (mstudioanimvalue_t*)((uint8_t*)panim + panim->offset[j + 3]);
			int k = frame;
			while (panimvalue->num.total <= k)
			{
				k -= panimvalue->num.total;
				panimvalue += panimvalue->num.valid + 1;
			}
			// Bah, missing blend!
			if (panimvalue->num.valid > k)
			{
				angle1[j] = panimvalue[k + 1].value;

				if (panimvalue->num.valid > k + 1)
				{
					angle2[j] = panimvalue[k + 2].value;
				}
				else
				{
					if (panimvalue->num.total > k + 1)
						angle2[j] = angle1[j];
					else
						angle2[j] = panimvalue[panimvalue->num.valid + 2].value;
				}
			}
			else
			{
				angle1[j] = panimvalue[panimvalue->num.valid].value;
				// TODO: I don't understand this code yet and ASAN complained about heap overflow here.
				// It crashed for an external sequence on the last frame and last bone. Removing +1 fixes
				// the crash but breaks animations (big momma, grunt)
				if (panimvalue->num.total > k + 1)
				{
					angle2[j] = angle1[j];
				}
				else
				{
					angle2[j] = panimvalue[panimvalue->num.valid + 2].value;
				}
			}
			angle1[j] = pbone->value[j + 3] + angle1[j] * pbone->scale[j + 3];
			angle2[j] = pbone->value[j + 3] + angle2[j] * pbone->scale[j + 3];
		}

		if (pbone->bonecontroller[j + 3] != -1)
		{
			angle1[j] += m_Adj[pbone->bonecontroller[j + 3]];
			angle2[j] += m_Adj[pbone->bonecontroller[j + 3]];
		}
	}

	if (!VectorCompare(angle1Vec, angle2Vec))
	{
		vec4 q1, q2;

		AngleQuaternion(angle1Vec, q1);
		AngleQuaternion(angle2Vec, q2);
		QuaternionSlerp(q1, q2, s, q);
	}
	else
	{
		AngleQuaternion(angle1Vec, q);
	}
}

void MdlRenderer::CalcBonePosition(const int frame, const float s, const mstudiobone_t* const pbone, const mstudioanim_t* const panim, float* pos)
{
	for (int j = 0; j < 3; j++)
	{
		pos[j] = pbone->value[j]; // default;
		if (panim->offset[j] != 0)
		{
			auto panimvalue = (mstudioanimvalue_t*)((uint8_t*)panim + panim->offset[j]);

			auto k = frame;
			// find span of values that includes the frame we want
			while (panimvalue->num.total <= k)
			{
				k -= panimvalue->num.total;
				panimvalue += panimvalue->num.valid + 1;
			}
			// if we're inside the span
			if (panimvalue->num.valid > k)
			{
				// and there's more data in the span
				if (panimvalue->num.valid > k + 1)
				{
					pos[j] += (panimvalue[k + 1].value * (1.0 - s) + s * panimvalue[k + 2].value) * pbone->scale[j];
				}
				else
				{
					pos[j] += panimvalue[k + 1].value * pbone->scale[j];
				}
			}
			else
			{
				// are we at the end of the repeating values section and there's another section with data?
				if (panimvalue->num.total <= k + 1)
				{
					pos[j] += (panimvalue[panimvalue->num.valid].value * (1.0 - s) + s * panimvalue[panimvalue->num.valid + 2].value) * pbone->scale[j];
				}
				else
				{
					pos[j] += panimvalue[panimvalue->num.valid].value * pbone->scale[j];
				}
			}
		}
		if (pbone->bonecontroller[j] != -1)
		{
			pos[j] += m_Adj[pbone->bonecontroller[j]];
		}
	}
}

void MdlRenderer::CalcBones(vec3* pos, vec4* q, const mstudioseqdesc_t* const pseqdesc, const mstudioanim_t* panim, const float f, bool isGait)
{
	const int frame = (int)f;
	const float s = (f - frame);

	// add in programatic controllers
	CalcBoneAdj();

	data.seek(header->boneindex);
	mstudiobone_t* pbones = (mstudiobone_t*)data.get();
	mstudiobone_t* pbone = (mstudiobone_t*)data.get();

	bool copy_bones = true;
	for (int i = 0; i < header->numbones; i++, pbone++, panim++)
	{
		if (isGait) {			
			if (!strcmp(pbone->name, "Bip01 Spine")) {
				// stop copying bones from the lower spine upwards
				copy_bones = false;
			}
			else if (pbone->parent != -1 && !strcmp(pbones[pbone->parent].name, "Bip01 Pelvis")) {
				// copy bones from the waist down
				copy_bones = true;
			}

			if (!copy_bones)
				continue;
		}

		CalcBoneQuaternion(frame, s, pbone, panim, q[i]);
		CalcBonePosition(frame, s, pbone, panim, (float*)&pos[i]);
	}

	if (pseqdesc->motiontype & STUDIO_X)
		pos[pseqdesc->motionbone].x = 0.0;
	if (pseqdesc->motiontype & STUDIO_Y)
		pos[pseqdesc->motionbone].y = 0.0;
	if (pseqdesc->motiontype & STUDIO_Z)
		pos[pseqdesc->motionbone].z = 0.0;
}

void MdlRenderer::SlerpBones(vec4* q1, vec3* pos1, vec4* q2, vec3* pos2, float s)
{
	vec4 q3;

	if (s < 0) s = 0;
	else if (s > 1.0) s = 1.0;

	const float s1 = 1.0 - s;

	for (int i = 0; i < header->numbones; i++)
	{
		QuaternionSlerp(q1[i], q2[i], s, q3);
		q1[i] = q3;

		pos1[i] = pos1[i] * s1 + pos2[i] * s;
	}
}

void MdlRenderer::QuaternionMatrix(float* quaternion, float matrix[3][4])
{
	matrix[0][0] = 1.0 - 2.0 * quaternion[1] * quaternion[1] - 2.0 * quaternion[2] * quaternion[2];
	matrix[1][0] = 2.0 * quaternion[0] * quaternion[1] + 2.0 * quaternion[3] * quaternion[2];
	matrix[2][0] = 2.0 * quaternion[0] * quaternion[2] - 2.0 * quaternion[3] * quaternion[1];

	matrix[0][1] = 2.0 * quaternion[0] * quaternion[1] - 2.0 * quaternion[3] * quaternion[2];
	matrix[1][1] = 1.0 - 2.0 * quaternion[0] * quaternion[0] - 2.0 * quaternion[2] * quaternion[2];
	matrix[2][1] = 2.0 * quaternion[1] * quaternion[2] + 2.0 * quaternion[3] * quaternion[0];

	matrix[0][2] = 2.0 * quaternion[0] * quaternion[2] + 2.0 * quaternion[3] * quaternion[1];
	matrix[1][2] = 2.0 * quaternion[1] * quaternion[2] - 2.0 * quaternion[3] * quaternion[0];
	matrix[2][2] = 1.0 - 2.0 * quaternion[0] * quaternion[0] - 2.0 * quaternion[1] * quaternion[1];
}

void R_ConcatTransforms(float in1[3][4], float in2[3][4], float out[3][4])
{
	out[0][0] = in1[0][0] * in2[0][0] + in1[0][1] * in2[1][0] + in1[0][2] * in2[2][0];
	out[0][1] = in1[0][0] * in2[0][1] + in1[0][1] * in2[1][1] + in1[0][2] * in2[2][1];
	out[0][2] = in1[0][0] * in2[0][2] + in1[0][1] * in2[1][2] + in1[0][2] * in2[2][2];
	out[0][3] = in1[0][0] * in2[0][3] + in1[0][1] * in2[1][3] + in1[0][2] * in2[2][3] + in1[0][3];
	out[1][0] = in1[1][0] * in2[0][0] + in1[1][1] * in2[1][0] + in1[1][2] * in2[2][0];
	out[1][1] = in1[1][0] * in2[0][1] + in1[1][1] * in2[1][1] + in1[1][2] * in2[2][1];
	out[1][2] = in1[1][0] * in2[0][2] + in1[1][1] * in2[1][2] + in1[1][2] * in2[2][2];
	out[1][3] = in1[1][0] * in2[0][3] + in1[1][1] * in2[1][3] + in1[1][2] * in2[2][3] + in1[1][3];
	out[2][0] = in1[2][0] * in2[0][0] + in1[2][1] * in2[1][0] + in1[2][2] * in2[2][0];
	out[2][1] = in1[2][0] * in2[0][1] + in1[2][1] * in2[1][1] + in1[2][2] * in2[2][1];
	out[2][2] = in1[2][0] * in2[0][2] + in1[2][1] * in2[1][2] + in1[2][2] * in2[2][2];
	out[2][3] = in1[2][0] * in2[0][3] + in1[2][1] * in2[1][3] + in1[2][2] * in2[2][3] + in1[2][3];
}

void MdlRenderer::SetUpBones(vec3 angles, int sequence, float frame, int gaitsequence, float gaitframe)
{
	angles = angles.flipToStudioMdl();

	sequence = clamp(sequence, 0, max(0, header->numseq-1));

	data.seek(header->seqindex + sequence * sizeof(mstudioseqdesc_t));
	mstudioseqdesc_t* pseqdesc = (mstudioseqdesc_t*)data.get();

	mstudioanim_t* panim = GetAnim(pseqdesc);

	//frame = clamp(frame, 0.0f, 1.0f) * (pseqdesc->numframes - 1.0f);
	frame = clamp(frame, 0.0f, pseqdesc->numframes - 1.0f);
	if (frame != frame) {
		frame = 0;
	}

	CalcBones(pos, q, pseqdesc, panim, frame, false);

	if (pseqdesc->numblends > 1)
	{
		panim += header->numbones;
		CalcBones(pos2, q2, pseqdesc, panim, frame, false);
		float s = iBlender[0] / 255.0;

		SlerpBones(q, pos, q2, pos2, s);

		if (pseqdesc->numblends == 4)
		{
			panim += header->numbones;
			CalcBones(pos3, q3, pseqdesc, panim, frame, false);

			panim += header->numbones;
			CalcBones(pos4, q4, pseqdesc, panim, frame, false);

			s = iBlender[0] / 255.0;
			SlerpBones(q3, pos3, q4, pos4, s);

			s = iBlender[1] / 255.0;
			SlerpBones(q, pos, q3, pos3, s);
		}
	}

	// calc gait animation
	if (gaitsequence >= 0 && gaitsequence < header->numseq)
	{
		data.seek(header->seqindex + gaitsequence * sizeof(mstudioseqdesc_t));
		mstudioseqdesc_t* gaitseqdesc = (mstudioseqdesc_t*)data.get();

		gaitframe = clamp(gaitframe, 0.0f, 1.0f) * (gaitseqdesc->numframes - 1.0f);

		data.seek(header->boneindex);
		mstudiobone_t* pbones = (mstudiobone_t*)data.get();

		mstudioanim_t* gaitanim = GetAnim(gaitseqdesc);
		CalcBones(pos, q, gaitseqdesc, gaitanim, gaitframe, true);
	}

	data.seek(header->boneindex);
	mstudiobone_t* pbones = (mstudiobone_t*)data.get();

	float bonematrix[3][4];

	float modelAngleMatrix[3][4];
	vec4 angleQuat;
	
	// TODO: this is triggering ASAN in release mode somehow. Access on angles/angleQuat is a stack overflow.
	// Fix by disabling this line. When multi-threaded it crashes somewhere else, but disabling this line still fixes it.
	AngleQuaternion(angles * (PI / 180.0f), angleQuat);

	for (int i = 0; i < header->numbones; i++)
	{
		QuaternionMatrix((float*)&q[i], bonematrix);

		bonematrix[0][3] = pos[i].x;
		bonematrix[1][3] = pos[i].y;
		bonematrix[2][3] = pos[i].z;

		if (pbones[i].parent == -1)
		{
			QuaternionMatrix((float*)&angleQuat, modelAngleMatrix);

			modelAngleMatrix[0][3] = 0;
			modelAngleMatrix[1][3] = 0;
			modelAngleMatrix[2][3] = 0;

			R_ConcatTransforms(modelAngleMatrix, bonematrix, m_bonetransform[i]);
		}
		else
		{
			R_ConcatTransforms(m_bonetransform[pbones[i].parent], bonematrix, m_bonetransform[i]);
		}
	}
}

void MdlRenderer::untransformVerts() {
	for (int b = 0; b < header->numbodyparts; b++) {
		data.seek(header->bodypartindex + b * sizeof(mstudiobodyparts_t));
		mstudiobodyparts_t* bod = (mstudiobodyparts_t*)data.get();

		for (int i = 0; i < bod->nummodels; i++) {
			data.seek(bod->modelindex + i * sizeof(mstudiomodel_t));
			mstudiomodel_t* mod = (mstudiomodel_t*)data.get();

			data.seek(mod->vertindex);
			vec3* pstudioverts = (vec3*)data.get();

			data.seek(mod->normindex);
			vec3* pstudionorms = (vec3*)data.get();

			data.seek(mod->vertinfoindex);
			uint8_t* pvertbone = (uint8_t*)data.get();

			data.seek(mod->norminfoindex);
			uint8_t* pnormbone = (uint8_t*)data.get();

			for (int k = 0; k < mod->numverts; k++) {
				transformedVerts[k] = pstudioverts[k];
			}
			for (int k = 0; k < mod->numnorms; k++) {
				transformedNormals[k] = pstudionorms[k];
			}

			for (int k = 0; k < mod->nummesh; k++) {
				MdlMeshRender& buffer = meshBuffers[b][i][k];

				for (int v = 0; v < buffer.numVerts; v++) {
					short oldVertIdx = buffer.origVerts[v];
					short oldNormIdx = buffer.origNorms[v];
					buffer.verts[v].pos = transformedVerts[oldVertIdx];
					buffer.verts[v].normal = transformedNormals[oldNormIdx];
				}
				buffer.buffer->upload();
			}
		}
	}
}

void MdlRenderer::transformVerts(int body, bool forRender, vec3 viewerOrigin, vec3 viewerRight) {
	int modelIdx = 0;

	int bodyValue = clamp(body, 0, 255);

	for (int b = 0; b < header->numbodyparts; b++) {
		data.seek(header->bodypartindex + b * sizeof(mstudiobodyparts_t));
		mstudiobodyparts_t* bod = (mstudiobodyparts_t*)data.get();

		int activeModel = (bodyValue / bod->base) % bod->nummodels;
		bodyValue -= activeModel * bod->base;

		//data.seek(bod->modelindex + activeModel * sizeof(mstudiomodel_t));
		//mstudiomodel_t* mod = (mstudiomodel_t*)data.get();

		for (int i = 0; i < bod->nummodels; i++) {
			data.seek(bod->modelindex + i * sizeof(mstudiomodel_t));
			mstudiomodel_t* mod = (mstudiomodel_t*)data.get();

			data.seek(mod->vertindex);
			vec3* pstudioverts = (vec3*)data.get();

			data.seek(mod->normindex);
			vec3* pstudionorms = (vec3*)data.get();

			data.seek(mod->vertinfoindex);
			uint8_t* pvertbone = (uint8_t*)data.get();

			data.seek(mod->norminfoindex);
			uint8_t* pnormbone = (uint8_t*)data.get();

			for (int k = 0; k < mod->numverts; k++) {
				VectorTransform(pstudioverts[k], m_bonetransform[pvertbone[k]], transformedVerts[k]);
			}

			if (forRender) {
				for (int k = 0; k < mod->numnorms; k++) {
					VectorRotate(pstudionorms[k], m_bonetransform[pnormbone[k]], transformedNormals[k]);
				}
			}

			for (int k = 0; k < mod->nummesh; k++) {
				MdlMeshRender& buffer = meshBuffers[b][i][k];

				if (forRender) {
					for (int v = 0; v < buffer.numVerts; v++) {
						short oldVertIdx = buffer.origVerts[v];
						short oldNormIdx = buffer.origNorms[v];
						buffer.verts[v].pos = transformedVerts[oldVertIdx];
						buffer.verts[v].normal = transformedNormals[oldNormIdx].flip();
					}

					if ((buffer.flags & STUDIO_NF_CHROME) && buffer.skinref < header->numtextures) {
						Texture* tex = glTextures[buffer.skinref];
						const float s = 1.0 / (float)tex->width;
						const float t = 1.0 / (float)tex->height;

						for (int v = 0; v < buffer.numVerts; v++) {
							vec3 tNormal = buffer.verts[v].normal;
							int boneIdx = (int)buffer.verts[v].bone;
							float (&bone)[4][4] = m_bonetransform[boneIdx];

							vec3 dir = (viewerOrigin + vec3(bone[0][3], bone[2][3], -bone[1][3])).normalize();

							vec3 chromeupvec = crossProduct(dir, viewerRight).normalize();
							vec3 chromerightvec = crossProduct(dir, chromeupvec).normalize();

							vec3 chromeup, chromeright;
							VectorIRotate(chromeupvec, bone, chromeup);
							VectorIRotate(chromerightvec, bone, chromeright);

							vec2 chrome;

							// calc s coord
							float n = dotProduct(tNormal, chromeright);
							buffer.verts[v].uv.x = ((n + 1.0) * 32.0) * s;

							// calc t coord
							n = dotProduct(tNormal, chromeup);
							buffer.verts[v].uv.y = ((n + 1.0) * 32.0) * t;
						}
					}

					buffer.buffer->upload();
				}
				else {
					for (int v = 0; v < buffer.numVerts; v++) {
						short oldVertIdx = buffer.origVerts[v];
						short oldNormIdx = buffer.origNorms[v];
						buffer.transformVerts[v] = transformedVerts[oldVertIdx].flipFromStudioMdl();
					}
				}
				
			}
		}
	}
}

void MdlRenderer::draw(vec3 origin, vec3 angles, EntRenderOpts& opts, vec3 viewerOrigin, vec3 viewerRight) {
	glCheckError("entering MDL render");
	
	if (!valid) {
		return;
	}

	bool legacyMode = legacy_mode;

	if (legacyMode != oldLegacyMode && !legacyMode) {
		SetUpBones(vec3(), opts.sequence, 0);
		untransformVerts();
		needTransform = true;
	}
	oldLegacyMode = legacyMode;

	uint64_t now = getEpochMillis();
	if (lastDrawCall == 0) {
		lastDrawCall = now;
	}
	float deltaTime = TimeDifference(lastDrawCall, now);
	lastDrawCall = now;

	opts.sequence = clamp(opts.sequence, 0, header->numseq - 1);
	mstudioseqdesc_t* seq = getSequence(opts.sequence);
	if (seq && seq->numframes > 1) {
		drawFrame += seq->fps * deltaTime;
		drawFrame = normalizeRangef(drawFrame, 0.0f, seq->numframes - 1);
	}

	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE);
	shader->bind();

	switch (opts.rendermode) {
	default:
	case RENDER_MODE_NORMAL:
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		shader->setUniform("colorMult", vec4(1, 1, 1, 1));
		break;
	case RENDER_MODE_SOLID:
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		shader->setUniform("colorMult", vec4(1, 1, 1, 1));
		break;
	case RENDER_MODE_COLOR:
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		shader->setUniform("colorMult", vec4(opts.rendercolor.toVec(), opts.renderamt / 255.0f));
		break;
	case RENDER_MODE_TEXTURE:
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		shader->setUniform("colorMult", vec4(1, 1, 1, opts.renderamt / 255.0f));
		break;
	case RENDER_MODE_GLOW:
		glBlendFunc(GL_SRC_ALPHA, GL_ONE);
		shader->setUniform("colorMult", vec4(1, 1, 1, opts.renderamt / 255.0f));
		break;
	case RENDER_MODE_ADDITIVE:
		glBlendFunc(GL_SRC_ALPHA, GL_ONE);
		shader->setUniform("colorMult", vec4(1, 1, 1, opts.renderamt / 255.0f));
		break;
	}

	shader->setUniform("sTex", 0);
	shader->setUniform("elights", 1); // number of active lights
	shader->setUniform("ambient", opts.rendercolor.toVec() * 0.4f); // ambient lighting
	
	if (!legacyMode) {
		shader->setUniform("viewerOrigin", viewerOrigin - origin); // world coordinates
		shader->setUniform("viewerRight", viewerRight);
	}

	// light data
	vec3 lights[4][3];

	for (int i = 0; i < 4; i++) {
		memset(lights[i], 0, 3*sizeof(vec3));
	}
	lights[0][0] = vec3(1024, 1024, 1024); // light position
	lights[0][1] = opts.rendercolor.toVec(); // diffuse color
	shader->setUniform("lights", (float*)lights, 4*3*3);
	glCheckError("setting MDL scene uniforms");

	shader->pushMatrix(MAT_MODEL);
	shader->modelMat->loadIdentity();
	shader->modelMat->translate(origin.x, origin.z, -origin.y);
	
	if (!legacyMode) {
		SetUpBones(angles, opts.sequence, drawFrame);

		// Hack: upload bone matrices as texture pixels.
		// Opengl 3.0 doesn't have uniform buffers and mat4[128] is far too many uniforms for a valid shader.
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, u_boneTexture);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 4, MAXSTUDIOBONES, GL_RGBA, GL_FLOAT, m_bonetransform);
		shader->setUniform("boneMatrixTexture", 1);
	}
	else {
		shader->setUniform("chromeEnable", 0);

		// no way to upload bone data. Do transforms on the CPU (slow!!)
		SetUpBones(angles, opts.sequence, drawFrame);
		transformVerts(opts.body, true);
		needTransform = true;
	}
	glActiveTexture(GL_TEXTURE0);

	// Don't rotate the scene because it messes up chrome effect.
	shader->updateMatrixes();

	glCheckError("updating MDL bone texture");

	data.seek(header->skinindex);
	
	int skin = clamp(opts.skin, 0, header->numskinfamilies-1);
	short* pskinref = (short*)data.get();

	int bodyValue = clamp(opts.body, 0, 255);

	for (int b = 0; b < header->numbodyparts; b++) {
		// Try loading required model info
		data.seek(header->bodypartindex + b * sizeof(mstudiobodyparts_t));
		mstudiobodyparts_t* bod = (mstudiobodyparts_t*)data.get();

		int activeModel = (bodyValue / bod->base) % bod->nummodels;
		bodyValue -= activeModel * bod->base;

		data.seek(bod->modelindex + activeModel * sizeof(mstudiomodel_t));
		mstudiomodel_t* mod = (mstudiomodel_t*)data.get();

		for (int k = 0; k < mod->nummesh; k++) {
			MdlMeshRender& render = meshBuffers[b][activeModel][k];
			
			short remappedSkin = pskinref[skin*header->numskinref + render.skinref];
			if (remappedSkin < 0 || remappedSkin >= header->numtextures) {
				remappedSkin = render.skinref;
			}

			Texture* tex = glTextures[remappedSkin];
			tex->bind();

			glCheckError("binding MDL texture");

			if (!render.buffer) {
				continue;
			}

			if (render.flags & STUDIO_NF_ADDITIVE) {
				shader->setUniform("additiveEnable", 1);
			}
			else {
				shader->setUniform("additiveEnable", 0);
			}

			int flatShade = 0;
			if (render.flags & STUDIO_NF_FULLBRIGHT) {
				flatShade = 2;
			} else if (render.flags & STUDIO_NF_FLATSHADE) {
				flatShade = 1;
			}

			shader->setUniform("flatshadeEnable", flatShade);

			if (!legacyMode) {
				if (render.flags & STUDIO_NF_CHROME) {
					const float s = 1.0 / (float)tex->width;
					const float t = 1.0 / (float)tex->height;

					shader->setUniform("chromeEnable", 1);
					shader->setUniform("textureST", vec2(s, t));
				}
				else {
					shader->setUniform("chromeEnable", 0);
				}
			}

			glCheckError("setting MDL body uniforms");

			render.buffer->draw(GL_TRIANGLES);

			glCheckError("rendering MDL body part");
		}
	}

	//printf("Draw %d meshes\n", meshCount);

	shader->popMatrix(MAT_MODEL);
}

// get a AABB containing all model vertices at the given angles and animation frame
void MdlRenderer::getModelBoundingBox(vec3 angles, int sequence, vec3& mins, vec3& maxs) {
	sequence = clamp(sequence, 0, header->numseq - 1);
	mstudioseqdesc_t* seq = getSequence(sequence);
	if (!seq) {
		mins = vec3();
		maxs = vec3();
		return;
	}

	mins = vec3(FLT_MAX, FLT_MAX, FLT_MAX);
	maxs = vec3(-FLT_MAX, -FLT_MAX, -FLT_MAX);

	if (cachedBounds[sequence].isCached) {
		mins = cachedBounds[sequence].mins;
		maxs = cachedBounds[sequence].maxs;
	}
	else {
		if (isEmpty()) {
			cachedBounds[sequence].mins = vec3();
			cachedBounds[sequence].maxs = vec3();
			cachedBounds[sequence].isCached = true;
			return;
		}

		for (int f = 0; f < seq->numframes; f++) {
			SetUpBones(vec3(), sequence, f);
			transformVerts(255, false);

			int meshCount = 0;
			for (int b = 0; b < header->numbodyparts; b++) {
				// Try loading required model info
				data.seek(header->bodypartindex + b * sizeof(mstudiobodyparts_t));
				mstudiobodyparts_t* bod = (mstudiobodyparts_t*)data.get();

				for (int i = 0; i < bod->nummodels && i < 1; i++) {
					data.seek(bod->modelindex + i * sizeof(mstudiomodel_t));
					mstudiomodel_t* mod = (mstudiomodel_t*)data.get();

					for (int k = 0; k < mod->nummesh; k++) {
						meshCount++;
						MdlMeshRender& render = meshBuffers[b][i][k];

						for (int v = 0; v < render.numVerts; v++) {
							expandBoundingBox(render.transformVerts[v], mins, maxs);
						}
					}
				}
			}
		}

		cachedBounds[sequence].mins = mins;
		cachedBounds[sequence].maxs = maxs;
		cachedBounds[sequence].isCached = true;
	}

	angles = angles.flip();

	// rotate the AABB then get a bounding box for the oriented box
	mat4x4 rotMatrix;
	rotMatrix.loadIdentity();
	rotMatrix.rotateX(angles.x * (PI/180.0f));
	rotMatrix.rotateY(angles.y * (PI/180.0f));
	rotMatrix.rotateZ(angles.z * (PI/180.0f));

	vec3 corners[8] = {
		vec3(mins.x, mins.y, mins.z),
		vec3(mins.x, mins.y, maxs.z),
		vec3(mins.x, maxs.y, mins.z),
		vec3(mins.x, maxs.y, maxs.z),
		vec3(maxs.x, mins.y, mins.z),
		vec3(maxs.x, mins.y, maxs.z),
		vec3(maxs.x, maxs.y, mins.z),
		vec3(maxs.x, maxs.y, maxs.z),
	};
	
	mins = vec3(FLT_MAX, FLT_MAX, FLT_MAX);
	maxs = vec3(-FLT_MAX, -FLT_MAX, -FLT_MAX);

	for (int i = 0; i < 8; i++) {
		vec4 oriented = rotMatrix * vec4(corners[i], 1.0f);
		expandBoundingBox(oriented.xyz(), mins, maxs);
	}
}
