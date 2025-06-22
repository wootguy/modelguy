#version 120
// object variables
uniform mat4 modelViewProjection;

// vertex variables
attribute vec3 vPosition;
attribute vec4 vColor;

// fragment variables
varying vec4 fColor;

void main()
{
	gl_Position = modelViewProjection * vec4(vPosition, 1);
	fColor = vColor;
}