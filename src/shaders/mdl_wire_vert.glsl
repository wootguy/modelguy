#version 120
#define STUDIO_NF_CHROME 0x02
#define STUDIO_NF_ADDITIVE 0x20

uniform mat4 modelViewProjection;

// skeleton
// Texture as an array of mat4 (poor man's UBO). Vertex Texture Fetch requires GL 3.0 or 2.1 w/ ARB
// Can't use UBO without upgrading to GL 3.1. Can't have 128 mat4 uniforms for all GPUs.
uniform sampler2D boneMatrixTexture;

// vertex variables
attribute vec3 vPosition;
attribute float vBone;

vec3 rotateVector(vec3 v, inout mat4 mat);
vec3 irotateVector(vec3 v, mat4 mat);

void main()
{
	mat4 bone;
	float boneCoord = (vBone / 128.0) + (1.0 / 512.0);
	bone[0] = texture2D(boneMatrixTexture, vec2(0.00 + (1.0 / 8.0), boneCoord));
	bone[1] = texture2D(boneMatrixTexture, vec2(0.25 + (1.0 / 8.0), boneCoord));
	bone[2] = texture2D(boneMatrixTexture, vec2(0.50 + (1.0 / 8.0), boneCoord));
	bone[3] = texture2D(boneMatrixTexture, vec2(0.75 + (1.0 / 8.0), boneCoord));

	vec3 pos = rotateVector(vPosition, bone) + vec3(bone[0][3], bone[2][3], -bone[1][3]);

	gl_Position = modelViewProjection * vec4(pos, 1);
}

vec3 rotateVector(vec3 v, inout mat4 mat)
{
	vec3 vout; 
	vout.x = dot(v, mat[0].xyz); 
	vout.z = -(dot(v, mat[1].xyz)); 
	vout.y = dot(v, mat[2].xyz); 
	return vout; 
}

vec3 irotateVector(vec3 v, mat4 mat)
{
	vec3 vout; 
	vout.x = v.x * mat[0][0] + v.y * mat[1][0] + v.z * mat[2][0];
	vout.y = v.x * mat[0][1] + v.y * mat[1][1] + v.z * mat[2][1];
	vout.z = v.x * mat[0][2] + v.y * mat[1][2] + v.z * mat[2][2];
	return vout; 
}
