#version 120
varying vec2 fTex;
varying vec4 fColor;

uniform vec4 colorMult;
uniform sampler2D sTex;

void main()
{
	vec4 texel = texture2D(sTex, fTex);
	if (texel.a < 0.5) discard; // GL_ALPHA_TEST alternative for WebGL (TODO: expensive. Don't use this for every mdl draw call)
	gl_FragColor = texel * fColor * colorMult;
}