#ifdef EMSCRIPTEN
#include <GLES3/gl3.h>
#else
#include <GL/glew.h>
#endif

#include "Shader.h"
#include "util.h"

Shader::Shader( const char * sourceCode, int shaderType )
{
#ifdef EMSCRIPTEN
	string sourceCopy = sourceCode;
	sourceCopy = replaceString(sourceCopy, "#version 120", "#version 100\nprecision highp float;\nprecision highp int;");
	sourceCode = sourceCopy.c_str();
#endif

	// Create Shader And Program Objects
	ID = glCreateShader(shaderType);

	glShaderSource(ID, 1, &sourceCode, NULL);
	glCompileShader(ID);

	const char* shaderTypeName = "<unknown type>";

	switch (shaderType) {
	case GL_VERTEX_SHADER:
		shaderTypeName = "Vertex";
		break;
	case GL_FRAGMENT_SHADER:
		shaderTypeName = "Fragment";
		break;
	}

	int success;
	glGetShaderiv(ID, GL_COMPILE_STATUS, &success);
	if (success != GL_TRUE)
	{
		char* log = new char[512];
		int len;
		glGetShaderInfoLog(ID, 512, &len, log);
		printf("Failed to compile %s shader\n", shaderTypeName);
		printf(log);
		printf("\n");
		if (len > 512)
			printf("Log too big to fit!");
		delete[] log;
	}

	compiled = success;
}


Shader::~Shader(void)
{
	glDeleteShader(ID);
}

