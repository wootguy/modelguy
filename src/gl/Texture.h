#pragma once
#include <stdint.h>
#include <vector>
#include "colors.h"

class Texture
{
public:	
	uint32_t id; // OpenGL texture ID
	int layer; // layer in the texture array. 0 if not in an array or at layer 0
	uint32_t height, width;
	uint8_t * data; // RGB(A) data
	int nearFilter;
	int farFilter;
	uint32_t format = 0; // format of the data
	bool uploaded = false;

	Texture(int width, int height);
	Texture(int width, int height, int depth);
	Texture(int width, int height, void * data);
	~Texture();

	// upload the texture with the specified settings
	void upload(int format);

	// use this texture for rendering
	void bind();
};