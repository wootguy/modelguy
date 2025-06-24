#include <GL/glew.h>
#include "Texture.h"

Texture::Texture(int width, int height) {
	this->width = width;
	this->height = height;
	this->nearFilter = GL_LINEAR;
	this->farFilter = GL_LINEAR_MIPMAP_LINEAR;
	this->data = new uint8_t[width*height*sizeof(COLOR4)];
	layer = 0;
}

Texture::Texture(int width, int height, int depth) {
	this->width = width;
	this->height = height;
	this->nearFilter = GL_LINEAR;
	this->farFilter = GL_LINEAR_MIPMAP_LINEAR;
	this->data = new uint8_t[width * height * depth * sizeof(COLOR4)];
	layer = 0;
}

Texture::Texture( int width, int height, void * data )
{
	this->width = width;
	this->height = height;
	this->nearFilter = GL_LINEAR;
	this->farFilter = GL_LINEAR_MIPMAP_LINEAR;
	this->data = (uint8_t*)data;
	layer = 0;
}

Texture::~Texture()
{
	if (uploaded)
		glDeleteTextures(1, &id);
	if (data)
		delete[] data;
}

void Texture::upload(int format)
{
	if (!data) {
		return;
	}

	if (uploaded) { 
		glDeleteTextures(1, &id);
	}
	glGenTextures(1, &id);

	glBindTexture(GL_TEXTURE_2D, id); // Binds this texture handle so we can load the data into it

#ifdef EMSCRIPTEN
	// required for NPOT textures
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
#else
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT); // Note: GL_CLAMP is significantly slower
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
#endif

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	if (format == GL_RGB) {
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	}

	glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
	//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);

	uploaded = true;
}

void Texture::bind()
{
	glBindTexture(GL_TEXTURE_2D, id);
}
