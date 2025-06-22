#include "Renderer.h"
#include "embedded_shaders.h"
#include "Model.h"
#include "util.h"
#include "MdlRenderer.h"
#include "primitives.h"

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

	window = glfwCreateWindow(500, 800, "modelguy", NULL, NULL);

	if (!window) {
		return false;
	}

	glfwSetWindowSizeLimits(window, 250, 50, GLFW_DONT_CARE, GLFW_DONT_CARE);
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

float Renderer::get_model_fit_distance(vec3 modelOrigin, vec3 modelAngles) {
	vec3 mins(FLT_MAX, FLT_MAX, FLT_MAX);
	vec3 maxs(-FLT_MAX, -FLT_MAX, -FLT_MAX);
	mdlRenderer->getModelBoundingBox(modelAngles, 0, mins, maxs);
	mins = mins.flip();
	maxs = maxs.flip();

	float targetWidth = max(max(fabs(maxs.x), fabs(mins.x)), max(fabs(maxs.z), fabs(mins.z))) * 2;
	float targetHeight = max(fabs(maxs.y) * 2, fabs(mins.y) * 2);

	// draw bounding box
	//colorShader->bind();
	//colorShader->updateMatrixes();
	//drawBoxOutline(modelOrigin.flip(), mins, maxs, COLOR4(0, 255, 0, 255));

	float aspect = (float)windowWidth / (float)windowHeight;
	float tanHalfFov = tan(fov * (PI / 180.0f) * 0.5f);
	float i_width = targetWidth / (2.0f * aspect * tanHalfFov);
	float i_height = targetHeight / (2.0f * tanHalfFov);

	return max(i_width, i_height) + (targetWidth * 0.5f);
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

	vec3 modelAngles = vec3(0, 0, 0);
	vec3 modelOrigin = vec3(0, 0, 0);

	vec3 ori;

	float lastFrameTime = glfwGetTime();
	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();

		glClearColor(0, 0, 64, 255);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glfwGetFramebufferSize(window, &windowWidth, &windowHeight);
		glViewport(0, 0, windowWidth, windowHeight);
		projection.perspective(fov, (float)windowWidth / (float)windowHeight, zNear, zFar);
		view.loadIdentity();
		model.loadIdentity();

		modelAngles.y = normalizeRangef(modelAngles.y + 0.5f, 0, 360);
		modelOrigin.y = max(modelOrigin.y, get_model_fit_distance(modelOrigin, modelAngles));
		mdlRenderer->draw(modelOrigin, modelAngles, renderOpts, cameraOrigin, cameraRight);

		glfwSwapBuffers(window);
		glCheckError("Swap buffers and controls");
	}

	glfwTerminate();
}

