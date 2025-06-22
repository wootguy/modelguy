#include "Renderer.h"
#include "embedded_shaders.h"
#include "Model.h"
#include "util.h"
#include "MdlRenderer.h"

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

Renderer::Renderer(string fpath, bool legacy_renderer) {
	this->legacy_renderer = legacy_renderer;

	create_window();

	mdlShader->bind();
	this->mdlRenderer = new MdlRenderer(mdlShader, legacy_renderer, fpath);
}

bool Renderer::create_window() {
	if (!glfwInit())
	{
		printf("GLFW initialization failed\n");
		return false;
	}

	glfwSetErrorCallback(error_callback);

	//glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);

	window = glfwCreateWindow(250, 400, "modelguy", NULL, NULL);

	if (!window) {
		return false;
	}

	glfwSetWindowSizeLimits(window, 500, 800, GLFW_DONT_CARE, GLFW_DONT_CARE);
	glfwMakeContextCurrent(window);
	
	glCheckError("glfw init");
	
	glewInit();

	glCheckError("glew init");

	// init to black screen instead of white
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// give ImGui something to push/pop to
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glfwSwapBuffers(window);
	glfwSwapInterval(1);

	glCheckError("glfw buffer setup");

	compile_shaders();

	glCheckError("compiling shaders");
}

void Renderer::compile_shaders() {

	const char* mdl_vert = legacy_renderer ? mdl_legacy_vert_glsl : mdl_vert_glsl;

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

void Renderer::render_loop() {
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_FRONT);
	glEnable(GL_TEXTURE_2D);
	glActiveTexture(GL_TEXTURE0);

	EntRenderOpts renderOpts;
	memset(&renderOpts, 0, sizeof(EntRenderOpts));
	renderOpts.scale = 1.0f;
	renderOpts.framerate = 1.0f;
	renderOpts.rendercolor = COLOR3(255, 255, 255);

	vec3 cameraOrigin = vec3();
	vec3 cameraRight = vec3(1, 0, 0);

	vec3 modelOrigin = vec3(0, 100, 0);
	vec3 modelAngles = vec3(0, 0, 0);

	float lastFrameTime = glfwGetTime();
	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();

		glClearColor(0, 0, 64, 255);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glfwGetFramebufferSize(window, &windowWidth, &windowHeight);

		glViewport(0, 0, windowWidth, windowHeight);

		projection.perspective(fov, (float)windowWidth / (float)windowHeight, zNear, zFar);

		view.loadIdentity();
		mdlRenderer->draw(modelOrigin, modelAngles, renderOpts, cameraOrigin, cameraRight);

		modelAngles.y = normalizeRangef(modelAngles.y + 0.5f, 0, 360);

		glfwSwapBuffers(window);
		glCheckError("Swap buffers and controls");
	}

	glfwTerminate();
}

