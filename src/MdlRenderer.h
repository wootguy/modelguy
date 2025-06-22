#pragma once
#include "studio.h"
#include "mstream.h"
#include "Texture.h"
#include "VertexBuffer.h"
#include <future>

#define	EQUAL_EPSILON	0.001f

struct MdlVert {
	vec3 pos;
	vec2 uv;
	vec4 color;
	short origVert;
	short origNorm;
};

struct boneVert {
	vec2 uv;
	vec3 normal;
	vec3 pos;
	float bone;
};

struct MdlMeshRender {
	boneVert* verts;
	short* origVerts; // original mdl vertex used to create the rendered vertex
	vec3* transformVerts; // duplicate of verts positions that can be edited before/after buffers upload
	short* origNorms; // original mdl normals used to create the rendered normal
	int numVerts;
	int flags;
	int skinref; // index into glTextures or remappable skin
	VertexBuffer* buffer;
};

struct EntRenderOpts {
	uint8_t rendermode;
	uint8_t renderamt;
	COLOR3 rendercolor;
	float framerate;
	float scale;
	int vp_type;
	int body;
	int skin;
	int sequence;
};

enum render_modes {
	RENDER_MODE_NORMAL,
	RENDER_MODE_COLOR,
	RENDER_MODE_TEXTURE,
	RENDER_MODE_GLOW,
	RENDER_MODE_SOLID,
	RENDER_MODE_ADDITIVE,
};

class Entity;

class MdlRenderer {
public:
	uint8_t iController[4];
	uint8_t iBlender[2];
	uint8_t iMouth;
	float m_Adj[MAXSTUDIOCONTROLLERS];

	// convenience state for rendering
	int lastSequence = 0;
	vec3 drawOrigin;
	int drawBody;

	MdlRenderer(ShaderProgram* shader, bool legacy_mode, string modelPath);
	~MdlRenderer();

	void draw(vec3 origin, vec3 angles, EntRenderOpts& opts, vec3 viewerOrigin, vec3 viewerRight);

	void upload(); // called by main thread to upload data to gpu

	// get a AABB containing all possible vertices in the given animation with given angles
	void getModelBoundingBox(vec3 angles, int sequence, vec3& mins, vec3& maxs);

	bool isStudioModel() { return true; };

	void loadData();

private:
	string fpath;
	bool valid;
	bool legacy_mode;

	ShaderProgram* shader;
	Texture** glTextures = NULL;
	MdlMeshRender*** meshBuffers = NULL;
	int numTextures;

	studiohdr_t* header = NULL;
	studiohdr_t* texheader = NULL; // external texture data
	mstream data; // TODO: parse structures into class members instead of seeking through the original data
	mstream texdata;
	vector<mstream> seqheaders; // external sequence model data

	bool oldLegacyMode;
	bool needTransform;

	uint u_boneTexture;

	struct AABB {
		vec3 mins, maxs;
		bool isCached;
	};
	AABB cachedBounds[MAXSTUDIOANIMATIONS]; // cached results for getModelBoundingBox

	// for setupbones
	vec3 pos[MAXSTUDIOBONES];
	vec4 q[MAXSTUDIOBONES];
	vec3 pos2[MAXSTUDIOBONES];
	vec4 q2[MAXSTUDIOBONES];
	vec3 pos3[MAXSTUDIOBONES];
	vec4 q3[MAXSTUDIOBONES];
	vec3 pos4[MAXSTUDIOBONES];
	vec4 q4[MAXSTUDIOBONES];

	// for transformverts
	vec3 transformedVerts[MAXSTUDIOVERTS];
	vec3 transformedNormals[MAXSTUDIOVERTS];

	float drawFrame = 0;
	uint64_t lastDrawCall = 0;

	bool loadTextureData();
	bool loadSequenceData();
	bool loadMeshes();
	void calcAnimBounds(); // calculate bounding boxes for anll animations
	bool isEmpty();
	bool validate();
	bool hasExternalTextures();
	bool hasExternalSequences();
	void transformVerts(int body, bool forRender, vec3 viewerOrigin=vec3(), vec3 viewerRight=vec3(1,0,0));
	void untransformVerts();

	// frame values = 0 - 1.0 (0-100%)
	// angles = rotation for the entire model (y = pitch, z = yaw)
	void SetUpBones(vec3 angles, int sequence, float frame, int gaitsequence = -1, float gaitframe = 0);
	
	float m_bonetransform[MAXSTUDIOBONES][4][4];	// bone transformation matrix (3x4)

	mstudioanim_t* GetAnim(mstudioseqdesc_t* pseqdesc);
	mstudioseqdesc_t* getSequence(int seq);

	// functions copied from Solokiller's model viewer
	void CalcBones(vec3* pos, vec4* q, const mstudioseqdesc_t* const pseqdesc, const mstudioanim_t* panim, const float f, bool isGait);
	void CalcBoneQuaternion(const int frame, const float s, const mstudiobone_t* const pbone, const mstudioanim_t* const panim, vec4& q);
	void CalcBonePosition(const int frame, const float s, const mstudiobone_t* const pbone, const mstudioanim_t* const panim, float* pos);
	void CalcBoneAdj();
	void SlerpBones(vec4* q1, vec3* pos1, vec4* q2, vec3* pos2, float s);
	void QuaternionMatrix(float* quaternion, float matrix[3][4]);};