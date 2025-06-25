#ifdef EMSCRIPTEN

#include <GLFW/glfw3.h>
#include "Renderer.h"
#include "util.h"

#ifdef WIN32
#define EMSCRIPTEN_KEEPALIVE
#define EM_ASM
#endif

#include <emscripten/emscripten.h>
#include <sys/time.h>
#include <time.h>
#include <deque>

float angleStep = 1.0f;

volatile bool newModelReady = false;
volatile bool shouldUnloadCurrentModel = false;
char newModelPath[1024];

int width = 500;
int height = 800;
bool g_paused = false;

uint64 lastFrameTime;
std::deque<float> framerates;
volatile bool program_ready = false;

Renderer* renderer;

void em_loop() {

	if (shouldUnloadCurrentModel) {
		printf("Unloading model\n");
		renderer->unload_model();
		shouldUnloadCurrentModel = false;
	}

	if (newModelReady) {
		printf("Loading newly downloaded model '%s'\n", newModelPath);
		newModelReady = false;

		if (renderer->load_model(newModelPath)) {
			EM_ASM( hlms_model_load_complete(true); );
		}
		else {
			EM_ASM( hlms_model_load_complete(false); );
		}
	}

	// for some reason this doesn't work in the main method
	if (!program_ready) {
		program_ready = true;
		EM_ASM(
			hlms_ready();
		);
	}

	if (g_paused) {
		return;
	}

	//printf("Dat time: %.1f\n", avgFramerate);

	renderer->render();

	//center_and_scale_model(settings);
}

bool file_exists(std::string fpath) {
	if (FILE* file = fopen(fpath.c_str(), "r"))
	{
		fclose(file);
		return true;
	}
	return false;
}


void download_file(std::string url, std::string local_file)
{
	if (file_exists(local_file))
	{
		printf("Loading %s from cache\n", local_file.c_str());
	}
	else {
		printf("Downloading file %s -> %s\n", url.c_str(), local_file.c_str());
		emscripten_wget(url.c_str(), local_file.c_str());
	}
}

// functions to be called from javascript
extern "C" {
	EMSCRIPTEN_KEEPALIVE void load_new_model(const char* model_folder_url, const char* model_name, const char* t_model, int seq_groups) {
		std::string model_folder_url_str(model_folder_url);
		std::string model_name_str(model_name);
		std::string mdl_path = model_name_str + ".mdl";
		std::string t_mdl_path = std::string(t_model);

		std::string mdl_url = model_folder_url_str + mdl_path;

		download_file(mdl_url, mdl_path);
		if (t_mdl_path.size() > 0) {
			download_file(model_folder_url_str + t_mdl_path, t_mdl_path);
		}
		for (int i = 1; i < seq_groups; i++) {
			std::string seq_mdl_path = model_name_str;
			if (i < 10)
				seq_mdl_path += "0";
			seq_mdl_path += std::to_string(i) + ".mdl";
			download_file(model_folder_url_str + seq_mdl_path, seq_mdl_path);
		}

		strcpy_safe(newModelPath, mdl_path.c_str(), 1024);
		newModelReady = true;
	}

	EMSCRIPTEN_KEEPALIVE void set_animation(int idx) {
		renderer->change_animation(idx);
	}

	EMSCRIPTEN_KEEPALIVE void set_body(int body) {
		renderer->renderOpts.body = body;
	}

	EMSCRIPTEN_KEEPALIVE void pause(int paused) {
		g_paused = paused != 0;
		renderer->reset_view();
	}

	EMSCRIPTEN_KEEPALIVE void set_wireframe(int wireframe) {
		renderer->renderOpts.wireframe = wireframe != 0;
	}

	EMSCRIPTEN_KEEPALIVE void reset_zoom() {
		//reset_scale();
	}

	EMSCRIPTEN_KEEPALIVE void unload_model() {
		shouldUnloadCurrentModel = true;
	}

	EMSCRIPTEN_KEEPALIVE void update_viewport(int newWidth, int newHeight) {
		width = newWidth;
		height = newHeight;
		renderer->resize_view(width, height);
		printf("Viewport resized: %dx%d\n", width, height);
	}
}

int main(int argc, char* argv[])
{
	bool legacy = false;
	renderer = new Renderer("", width, height, legacy, false);
	renderer->setup_render();

	emscripten_set_main_loop(em_loop, 0, 1);

	return 0;
}

#endif