#include "ShaderProgram.h"
#include "colors.h"
#include "MdlRenderer.h"

class Model;
class GLFWwindow;

class Renderer {
public:
	Renderer(std::string fpath, int width, int height, bool legacy_renderer, bool headless);

	void render_loop();

	void create_image(string outPath);

private:
	string fpath;
	Model* m_model;
	MdlRenderer* mdlRenderer;
	bool legacy_renderer;
	bool headless;
	bool valid;
	uint8_t* mesa3d_buffer;

	GLFWwindow* window;
	ShaderProgram* mdlShader = NULL;
	ShaderProgram* colorShader = NULL;
	int windowWidth;
	int windowHeight;
	float fov = 75.0f;
	float zNear = 1.0f;
	float zFar = 262144.0f;

	mat4x4 model = mat4x4();
	mat4x4 view = mat4x4();
	mat4x4 projection = mat4x4();
	mat4x4 modelView = mat4x4();
	mat4x4 modelViewProjection = mat4x4();

	vec3 cameraOrigin = vec3();
	vec3 cameraRight = vec3(1, 0, 0);

	vec3 modelAngles = vec3(0, -90, 0);
	vec3 modelOrigin = vec3(0, 0, 0);

	EntRenderOpts renderOpts;

	void setup_render();
	void render();
	bool create_window(int width, int height);
	bool create_headless_context(int width, int height);
	void init_gl();
	void compile_shaders();
	float get_model_fit_distance(vec3 modelOrigin, vec3 modelAngles); // get distance needed to fit the model to the rendering window
	void drawBoxOutline(vec3 center, vec3 mins, vec3 maxs, COLOR4 color);
};