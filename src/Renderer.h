#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include "ShaderProgram.h"

class Model;
class MdlRenderer;

class Renderer {
public:
	Renderer(std::string fpath, bool legacy_renderer);

	void render_loop();

private:
	Model* m_model;
	MdlRenderer* mdlRenderer;
	bool legacy_renderer;


	GLFWwindow* window;
	ShaderProgram* mdlShader = NULL;
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

	bool create_window();
	void compile_shaders();
};