#version 120
#define STUDIO_NF_CHROME 0x02
#define STUDIO_NF_ADDITIVE 0x20

uniform mat4 modelViewProjection;

// Lighting uniforms
uniform mat4 modelView;
uniform mat4 normalMat;
uniform mat3 lights[4];
uniform int elights;
uniform vec3 ambient;

// skeleton
// Texture as an array of mat4 (poor man's UBO). Vertex Texture Fetch requires GL 3.0 or 2.1 w/ ARB
// Can't use UBO without upgrading to GL 3.1. Can't have 128 mat4 uniforms for all GPUs.
uniform sampler2D boneMatrixTexture;

// render flags
uniform int chromeEnable;
uniform int additiveEnable;
uniform int flatshadeEnable;

// chrome uniforms
uniform vec3 viewerOrigin;
uniform vec3 viewerRight;
uniform vec2 textureST;

// vertex variables
attribute vec3 vPosition;
attribute vec3 vNormal;
attribute vec2 vTex;
attribute float vBone;

// fragment variables
varying vec2 fTex;
varying vec4 fColor;

vec4 lighting(inout vec3 tNormal);
vec3 rotateVector(vec3 v, inout mat4 mat);
vec3 irotateVector(vec3 v, mat4 mat);
vec2 chrome(vec3 tNormal, inout mat4 bone);

void main()
{
	mat4 bone;
	float boneCoord = (vBone / 128.0) + (1.0 / 512.0);
	bone[0] = texture2D(boneMatrixTexture, vec2(0.00 + (1.0 / 8.0), boneCoord));
	bone[1] = texture2D(boneMatrixTexture, vec2(0.25 + (1.0 / 8.0), boneCoord));
	bone[2] = texture2D(boneMatrixTexture, vec2(0.50 + (1.0 / 8.0), boneCoord));
	bone[3] = texture2D(boneMatrixTexture, vec2(0.75 + (1.0 / 8.0), boneCoord));


	vec3 pos = rotateVector(vPosition, bone) + vec3(bone[0][3], bone[2][3], -bone[1][3]);
	vec3 tNormal = rotateVector(vNormal, bone);

	gl_Position = modelViewProjection * vec4(pos, 1);

	if (chromeEnable != 0) {
		fTex = chrome(vNormal, bone);
	} else {
		fTex = vTex;
	}

	// TODO: compile multiple shaders and control this if #ifdef
	if (additiveEnable != 0) {
		fColor = vec4(1, 1, 1, 0.5);
	} else if (flatshadeEnable == 1) {
		fColor = vec4(ambient, 1);
	} else if (flatshadeEnable == 2) {
		fColor = vec4(1, 1, 1, 1);
	} else {
		fColor = lighting(tNormal);
	}
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

vec2 chrome(vec3 tNormal, inout mat4 bone)
{
	vec3 dir = normalize(viewerOrigin + vec3(bone[0][3], bone[2][3], -bone[1][3]));

	vec3 chromeupvec = normalize(cross(dir, viewerRight));
	vec3 chromerightvec = normalize(cross(dir, chromeupvec));

	vec3 chromeup = irotateVector(chromeupvec, bone);
	vec3 chromeright = irotateVector(chromerightvec, bone);

	vec2 chrome;

// calc s coord
	float n = dot(tNormal, chromeright);
	chrome.x = ((n + 1.0) * 32.0) * textureST.x;

// calc t coord
	n = dot(tNormal, chromeup);
	chrome.y = ((n + 1.0) * 32.0) * textureST.y;

	return chrome;
}

vec4 lighting(inout vec3 tNormal)
{
	vec3 finalColor = ambient;
	for (int i = 0; i < 4; ++i)
	{
		if (i == elights) // Webgl won't let us use variables in our loop condition. So we have this.
			break;
		vec3 lightDirection = normalize(lights[i][0].xyz);
		vec3 diffuse = lights[i][1].xyz;

		finalColor += diffuse * max(0.0, dot(tNormal, lightDirection));
	}
	return vec4(clamp(finalColor, vec3(0, 0, 0), vec3(1, 1, 1)), 1);
}