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

vec4 lighting(vec3 tNormal);

void main()
{
	gl_Position = modelViewProjection * vec4(vPosition, 1);

	if (chromeEnable != 0) {
		fTex.x = vBone; // just keeping the var active to stop error spam
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
		fColor = lighting(vNormal);
	}
}

vec4 lighting(vec3 tNormal)
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