#version 120
uniform mat4 modelViewProjection;

// vertex variables
attribute vec3 vPosition;
attribute float vBone;

void main()
{
	gl_Position = modelViewProjection * vec4(vPosition, 1);

	if (vBone < -1000.0) {
		gl_Position.x += vBone; // just keeping the var active to stop error spam (chrome uniform is never enabled)
	}
}
