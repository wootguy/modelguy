#include "Renderer.h"
#include "embedded_shaders.h"
#include "Model.h"
#include "util.h"
#include "MdlRenderer.h"
#include "primitives.h"
#include "lodepng.h"
#include <cfloat>

#include <GL/glew.h>

#ifdef WIN32
#include <GLFW/glfw3.h>
#else
#define GLAPI extern
#include <GL/osmesa.h>
#endif

Renderer* g_app;

int glGetErrorDebug() {
	return glGetError();
}

void glCheckError(const char* checkMessage) {
	// error checking is very expensive
#ifdef DEBUG_MODE
	static int lastError = 0;
	int glerror = glGetError();
	if (glerror != GL_NO_ERROR) {
		printf("Got OpenGL Error %d after %s\n", glerror, checkMessage);
		lastError = glerror;
	}
#endif
}

void error_callback(int error, const char* description)
{
	printf("GLFW Error: %s\n", description);
}

void file_drop_callback(GLFWwindow* window, int count, const char** paths) {
	g_app->load_model(paths[0]);
}

Renderer::Renderer(string fpath, int width, int height, bool legacy_renderer, bool headless) {
	this->legacy_renderer = legacy_renderer;
	this->headless = headless;
	this->windowWidth = width;
	this->windowHeight = height;
	g_app = this;

	if (headless) {
#ifdef WIN32
		valid = create_window(width, height);
#else
		valid = create_headless_context(width, height);
#endif
	}
	else
		valid = create_window(width, height);

	if (valid) {
		init_gl();
		mdlShader->bind();
		load_model(fpath);
	}
}

void Renderer::load_model(std::string fpath) {
	this->fpath = fpath;
	if (mdlRenderer) {
		delete mdlRenderer;
	}
	mdlRenderer = new MdlRenderer(mdlShader, legacy_renderer, fpath);

	modelOrigin = vec3();
	modelAngles = vec3(0, -90, 0);
}

void Renderer::init_gl() {
	// init to black screen instead of white
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

#ifdef WIN32
	glfwSwapBuffers(window);
	glfwSwapInterval(1);
#endif

	glCheckError("glfw buffer setup");

	compile_shaders();

	glCheckError("compiling shaders");
}

bool Renderer::create_window(int width, int height) {
#ifdef WIN32
	if (!glfwInit())
	{
		printf("GLFW initialization failed\n");
		return false;
	}

	//glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);

	if (headless) {
		glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
	}

	window = glfwCreateWindow(width, height, "modelguy", NULL, NULL);

	if (!window) {
		return false;
	}

	glfwSetErrorCallback(error_callback);
	glfwSetDropCallback(window, file_drop_callback);

	glfwSetWindowSizeLimits(window, 250, 50, GLFW_DONT_CARE, GLFW_DONT_CARE);
	glfwMakeContextCurrent(window);

	GLenum err = glewInit();
	if (err != GLEW_OK) {
		printf("Failed to initialize GLEW: %s\n", glewGetErrorString(err));
		return false;
	}
#endif
	return true;
}

bool Renderer::create_headless_context(int width, int height) {
#ifndef WIN32
	const int attribs[] = {
		OSMESA_FORMAT, OSMESA_RGBA,
		OSMESA_DEPTH_BITS, 16,
		OSMESA_PROFILE, OSMESA_COMPAT_PROFILE,
		0
	};
	OSMesaContext ctx = OSMesaCreateContextAttribs(attribs, NULL);

	if (!ctx) {
		printf("OSMesaCreateContext failed!\n");
		return false;
	}

	/* Allocate the image buffer */
	mesa3d_buffer = (unsigned char*)aligned_alloc(16, width * height * 4);
	if (!mesa3d_buffer) {
		printf("Alloc image buffer failed!\n");
		return false;
	}

	/* Bind the buffer to the context and make it current */
	if (!OSMesaMakeCurrent(ctx, mesa3d_buffer, GL_UNSIGNED_BYTE, width, height)) {
		printf("OSMesaMakeCurrent failed!\n");
		return false;
	}

	//GLenum err = glewInit();
	//if (err != GLEW_OK) {
		//printf("Failed to initialize GLEW: %s\n", glewGetErrorString(err));
		//return;
	//}

	// GLEW is failing to initialize and I don't know why, so load gl functions manually
	glCreateShader = (PFNGLCREATESHADERPROC)OSMesaGetProcAddress("glCreateShader");
	glShaderSource = (PFNGLSHADERSOURCEPROC)OSMesaGetProcAddress("glShaderSource");
	glCompileShader = (PFNGLCOMPILESHADERPROC)OSMesaGetProcAddress("glCompileShader");
	glGetShaderiv = (PFNGLGETSHADERIVPROC)OSMesaGetProcAddress("glGetShaderiv");
	glGetShaderInfoLog = (PFNGLGETSHADERINFOLOGPROC)OSMesaGetProcAddress("glGetShaderInfoLog");
	glDeleteShader = (PFNGLDELETESHADERPROC)OSMesaGetProcAddress("glDeleteShader");
	glCreateProgram = (PFNGLCREATEPROGRAMPROC)OSMesaGetProcAddress("glCreateProgram");
	glAttachShader = (PFNGLATTACHSHADERPROC)OSMesaGetProcAddress("glAttachShader");
	glLinkProgram = (PFNGLLINKPROGRAMPROC)OSMesaGetProcAddress("glLinkProgram");
	glUseProgram = (PFNGLUSEPROGRAMPROC)OSMesaGetProcAddress("glUseProgram");
	glDeleteProgram = (PFNGLDELETEPROGRAMPROC)OSMesaGetProcAddress("glDeleteProgram");
	glDetachShader = (PFNGLDETACHSHADERPROC)OSMesaGetProcAddress("glDetachShader");
	glUniform1f = (PFNGLUNIFORM1FPROC)OSMesaGetProcAddress("glUniform1f");
	glUniform2f = (PFNGLUNIFORM2FPROC)OSMesaGetProcAddress("glUniform2f");
	glUniform3f = (PFNGLUNIFORM3FPROC)OSMesaGetProcAddress("glUniform3f");
	glUniform4f = (PFNGLUNIFORM4FPROC)OSMesaGetProcAddress("glUniform4f");
	glUniform1i = (PFNGLUNIFORM1IPROC)OSMesaGetProcAddress("glUniform1i");
	glUniform2i = (PFNGLUNIFORM2IPROC)OSMesaGetProcAddress("glUniform2i");
	glUniform3i = (PFNGLUNIFORM3IPROC)OSMesaGetProcAddress("glUniform3i");
	glUniform4i = (PFNGLUNIFORM4IPROC)OSMesaGetProcAddress("glUniform4i");
	glUniformMatrix2fv = (PFNGLUNIFORMMATRIX2FVPROC)OSMesaGetProcAddress("glUniformMatrix2fv");
	glUniformMatrix3fv = (PFNGLUNIFORMMATRIX3FVPROC)OSMesaGetProcAddress("glUniformMatrix3fv");
	glUniformMatrix4fv = (PFNGLUNIFORMMATRIX4FVPROC)OSMesaGetProcAddress("glUniformMatrix4fv");
	glGetUniformLocation = (PFNGLGETUNIFORMLOCATIONPROC)OSMesaGetProcAddress("glGetUniformLocation");
	glGetAttribLocation = (PFNGLGETATTRIBLOCATIONPROC)OSMesaGetProcAddress("glGetAttribLocation");
	glGetProgramiv = (PFNGLGETPROGRAMIVPROC)OSMesaGetProcAddress("glGetProgramiv");
	glGetProgramInfoLog = (PFNGLGETPROGRAMINFOLOGPROC)OSMesaGetProcAddress("glGetProgramInfoLog");
	glGenVertexArrays = (PFNGLGENVERTEXARRAYSPROC)OSMesaGetProcAddress("glGenVertexArrays");
	glBindVertexArray = (PFNGLBINDVERTEXARRAYPROC)OSMesaGetProcAddress("glBindVertexArray");
	glGenBuffers = (PFNGLGENBUFFERSPROC)OSMesaGetProcAddress("glGenBuffers");
	glBindBuffer = (PFNGLBINDBUFFERPROC)OSMesaGetProcAddress("glBindBuffer");
	glBufferData = (PFNGLBUFFERDATAPROC)OSMesaGetProcAddress("glBufferData");
	glBufferSubData = (PFNGLBUFFERSUBDATAPROC)OSMesaGetProcAddress("glBufferSubData");
	glDeleteBuffers = (PFNGLDELETEBUFFERSPROC)OSMesaGetProcAddress("glDeleteBuffers");
	glEnableVertexAttribArray = (PFNGLENABLEVERTEXATTRIBARRAYPROC)OSMesaGetProcAddress("glEnableVertexAttribArray");
	glDisableVertexAttribArray = (PFNGLDISABLEVERTEXATTRIBARRAYPROC)OSMesaGetProcAddress("glDisableVertexAttribArray");
	glVertexAttribPointer = (PFNGLVERTEXATTRIBPOINTERPROC)OSMesaGetProcAddress("glVertexAttribPointer");
	glActiveTexture = (PFNGLACTIVETEXTUREPROC)OSMesaGetProcAddress("glActiveTexture");
#endif

	return true;
}

void Renderer::compile_shaders() {

	const char* mdl_vert = legacy_renderer ? mdl_legacy_vert_glsl : mdl_vert_glsl;

	colorShader = new ShaderProgram("Color");
	colorShader->compile(cvert_vert_glsl, cvert_frag_glsl);
	colorShader->setMatrixes(&model, &view, &projection, &modelView, &modelViewProjection);
	colorShader->setMatrixNames(NULL, "modelViewProjection");
	colorShader->setVertexAttributeNames("vPosition", "vColor", NULL, NULL);

	mdlShader = new ShaderProgram("MDL");
	mdlShader->compile(mdl_vert, mdl_frag_glsl);
	mdlShader->setMatrixes(&model, &view, &projection, &modelView, &modelViewProjection);
	mdlShader->setMatrixNames(NULL, "modelViewProjection");
	mdlShader->setVertexAttributeNames("vPosition", NULL, "vTex", "vNormal");
	mdlShader->addUniform("sTex", UNIFORM_INT);
	mdlShader->addUniform("elights", UNIFORM_INT);
	mdlShader->addUniform("ambient", UNIFORM_VEC3);
	mdlShader->addUniform("lights", UNIFORM_MAT3);
	mdlShader->addUniform("additiveEnable", UNIFORM_INT);
	mdlShader->addUniform("chromeEnable", UNIFORM_INT);
	mdlShader->addUniform("flatshadeEnable", UNIFORM_INT);
	mdlShader->addUniform("colorMult", UNIFORM_VEC4);
	if (!legacy_renderer) {
		mdlShader->addUniform("viewerOrigin", UNIFORM_VEC3);
		mdlShader->addUniform("viewerRight", UNIFORM_VEC3);
		mdlShader->addUniform("textureST", UNIFORM_VEC2);
		mdlShader->addUniform("boneMatrixTexture", UNIFORM_INT);
	}
}

void Renderer::drawBoxOutline(vec3 center, vec3 mins, vec3 maxs, COLOR4 color) {
	mins += center;
	maxs += center;

	vec3 corners[8] = {
		vec3(mins.x, mins.y, mins.z), // 0
		vec3(maxs.x, mins.y, mins.z), // 1
		vec3(mins.x, maxs.y, mins.z), // 2
		vec3(maxs.x, maxs.y, mins.z), // 3
		vec3(mins.x, mins.y, maxs.z), // 4
		vec3(maxs.x, mins.y, maxs.z), // 5
		vec3(mins.x, maxs.y, maxs.z), // 6
		vec3(maxs.x, maxs.y, maxs.z),  // 7
	};

	cVert edges[24] = {
		cVert(corners[0], color), cVert(corners[1], color),
		cVert(corners[1], color), cVert(corners[3], color),
		cVert(corners[3], color), cVert(corners[2], color),
		cVert(corners[2], color), cVert(corners[0], color),
		cVert(corners[4], color), cVert(corners[5], color),
		cVert(corners[5], color), cVert(corners[7], color),
		cVert(corners[7], color), cVert(corners[6], color),
		cVert(corners[6], color), cVert(corners[4], color),
		cVert(corners[0], color), cVert(corners[4], color),
		cVert(corners[1], color), cVert(corners[5], color),
		cVert(corners[2], color), cVert(corners[6], color),
		cVert(corners[3], color), cVert(corners[7], color),
	};

	VertexBuffer buffer(colorShader, COLOR_4B | POS_3F, &edges, 24);
	buffer.upload();
	buffer.draw(GL_LINES);
}

void Renderer::get_model_fit_offsets(vec3 modelOrigin, vec3 modelAngles, float& depthOffset, float& heightOffset) {
	vec3 mins(FLT_MAX, FLT_MAX, FLT_MAX);
	vec3 maxs(-FLT_MAX, -FLT_MAX, -FLT_MAX);
	mdlRenderer->getModelBoundingBox(modelAngles, 0, mins, maxs);
	mins = mins.flip();
	maxs = maxs.flip();

	float targetWidth = max(fabs(maxs.x), fabs(mins.x)) * 2;
	float targetHeight = maxs.y - mins.y;

	// draw bounding box
	//colorShader->bind();
	//colorShader->updateMatrixes();
	//drawBoxOutline(modelOrigin.flip(), mins, maxs, COLOR4(0, 255, 0, 255));

	float aspect = (float)windowWidth / (float)windowHeight;
	float tanHalfFov = tan(fov * (PI / 180.0f) * 0.5f);
	float i_width = targetWidth / (2.0f * aspect * tanHalfFov);
	float i_height = targetHeight / (2.0f * tanHalfFov);

	depthOffset = mins.z + max(i_width, i_height);
	heightOffset = -(mins.y + (maxs.y - mins.y)*0.5f);
}

void Renderer::setup_render() {
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glEnable(GL_TEXTURE_2D);
	glActiveTexture(GL_TEXTURE0);

	memset(&renderOpts, 0, sizeof(EntRenderOpts));
	renderOpts.scale = 1.0f;
	renderOpts.framerate = 1.0f;
	renderOpts.rendercolor = COLOR3(255, 255, 255);
	renderOpts.body = 255; // default to "cl_himodels = 1" body

	glCullFace(headless ? GL_BACK : GL_FRONT);
}

void Renderer::render() {
#ifdef WIN32
	glfwGetFramebufferSize(window, &windowWidth, &windowHeight);
#endif

	glViewport(0, 0, windowWidth, windowHeight);
	projection.perspective(fov, (float)windowWidth / (float)windowHeight, zNear, zFar);
	view.loadIdentity();
	model.loadIdentity();

	glClearColor(0, 0, 0, 0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	if (headless)
		projection(5) *= -1;

	modelAngles.y = normalizeRangef(modelAngles.y + 0.5f, 0, 360);

	float depthOffset;
	get_model_fit_offsets(modelOrigin, modelAngles, depthOffset, modelOrigin.z);
	modelOrigin.y = max(modelOrigin.y, depthOffset);
	mdlRenderer->draw(modelOrigin, modelAngles, renderOpts, cameraOrigin, cameraRight);
}

void Renderer::render_loop() {
	if (!valid) {
		printf("Aborting render. Context creation failed.\n");
		return;
	}

#ifdef WIN32
	setup_render();

	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();

		render();

		glfwSwapBuffers(window);
		glCheckError("Swap buffers and controls");
	}

	glfwTerminate();
#endif
}

void Renderer::create_image(string outPath) {
	if (!valid) {
		printf("Aborting render. Context creation failed.\n");
		return;
	}
	setup_render();
	render();
	glFlush();

#ifdef WIN32
	uint8_t* pixels = new uint8_t[windowWidth * windowHeight * 4];
	glReadPixels(0, 0, windowWidth, windowHeight, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
	lodepng_encode32_file(outPath.c_str(), pixels, windowWidth, windowHeight);
	delete[] pixels;
#else
	lodepng_encode32_file(outPath.c_str(), mesa3d_buffer, windowWidth, windowHeight);
#endif
}
