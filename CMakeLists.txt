cmake_minimum_required(VERSION 3.6)
project(modelguy)

if (MSVC)
	# static runtime for all projects
	add_compile_options(/MT)
endif()

set(SOURCE_FILES 
    src/main.cpp
    src/main_ems.cpp
    src/Model.cpp		src/Model.h
    src/ModelType.cpp	src/ModelType.h
	src/studio.h
	src/types.h
	
	src/util/util.cpp		src/util/util.h
    src/util/mstream.cpp	src/util/mstream.h
    src/util/mat4x4.cpp		src/util/mat4x4.h
    src/util/colors.cpp		src/util/colors.h
    src/util/vectors.cpp	src/util/vectors.h
	
	src/Renderer.cpp		src/Renderer.h
	src/MdlRenderer.cpp		src/MdlRenderer.h
	
	src/gl/primitives.cpp		src/gl/primitives.h
	src/gl/Shader.cpp			src/gl/Shader.h
	src/gl/ShaderProgram.cpp	src/gl/ShaderProgram.h
	src/gl/Texture.cpp			src/gl/Texture.h
	src/gl/VertexBuffer.cpp		src/gl/VertexBuffer.h
	
	src/lib/json.hpp
	src/lib/md5.h		src/lib/md5.cpp
	src/lib/lodepng.h	src/lib/lodepng.cpp
)

set(SHADER_FILES
	src/shaders/cvert_vert.glsl
	src/shaders/cvert_frag.glsl
	
	src/shaders/mdl_vert.glsl
	src/shaders/mdl_frag.glsl
	src/shaders/mdl_legacy_vert.glsl
	
	src/shaders/mdl_wire_vert.glsl
	src/shaders/mdl_wire_legacy_vert.glsl
	src/shaders/mdl_wire_frag.glsl
)

# Custom command that always runs because cmake doesn't care if .glsl contents change.
# The command will check if the generated file needs updating or not
add_custom_command(
	OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/__dummy_file.h  # fake file that never exists, causing this step to run every build
	${CMAKE_BINARY_DIR}/embedded_shaders.cpp
	COMMAND ${CMAKE_COMMAND} -P "${CMAKE_SOURCE_DIR}/embed_shaders.cmake" "embedded_shaders.h" "embedded_shaders.cpp" ${SHADER_FILES}
)
add_custom_target(
	my_custom_target_that_always_runs ALL
	DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/__dummy_file.h
)

set(SOURCE_FILES ${SOURCE_FILES} "${CMAKE_BINARY_DIR}/embedded_shaders.cpp")

add_executable(${PROJECT_NAME} ${SOURCE_FILES})

include_directories(src)
include_directories(src/util)
include_directories(src/gl)
include_directories(src/viewer)
include_directories(src/lib)
include_directories(glew/include)
include_directories(${CMAKE_BINARY_DIR})

if(MSVC)
    # compile using the static runtime
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT /wd4244 /wd4018")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd /wd4244 /wd4018 -DDEBUG_MODE")
	
	add_definitions(-DGLEW_STATIC)
	
	add_subdirectory(glfw)
	
	if(CMAKE_SIZEOF_VOID_P EQUAL 4)
		set(GLEW_PATH ${CMAKE_CURRENT_SOURCE_DIR}/glew/lib/Release/Win32/glew32s.lib)
	else()
		set(GLEW_PATH ${CMAKE_CURRENT_SOURCE_DIR}/glew/lib/Release/x64/glew32s.lib)
	endif()

	target_link_libraries(${PROJECT_NAME} glfw opengl32 ${GLEW_PATH})
	
	# Disable C++ exceptions
	string(REPLACE "/EHsc" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
	set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT modelguy)
	
	source_group("Header Files\\gl" FILES	src/gl/Shader.h
											src/gl/ShaderProgram.h
											src/gl/VertexBuffer.h
											src/gl/Texture.h
											src/gl/primitives.h
											${CMAKE_BINARY_DIR}/embedded_shaders.h
											src/gl/shaders.h)
											
	source_group("Source Files\\gl" FILES	src/gl/Shader.cpp
											src/gl/ShaderProgram.cpp
											src/gl/VertexBuffer.cpp
											src/gl/Texture.cpp
											src/gl/primitives.cpp
											${CMAKE_BINARY_DIR}/embedded_shaders.cpp
											src/gl/shaders.cpp)
											
	source_group("Header Files\\util" FILES	src/util/util.h
											src/util/mstream.h
											src/util/mat4x4.h
											src/util/colors.h
											src/util/vectors.h)
											
	source_group("Source Files\\util" FILES	src/util/util.cpp
											src/util/mstream.cpp
											src/util/mat4x4.cpp
											src/util/colors.cpp
											src/util/vectors.cpp)

elseif(EMSCRIPTEN)		
	set(CMAKE_EXECUTABLE_SUFFIX ".html")
	set(SHELL_FILE ${CMAKE_CURRENT_SOURCE_DIR}/src/shell.html)
	
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2")
	set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS 
		"-sTOTAL_MEMORY=67108864 -sUSE_GLFW=3 -sASYNCIFY=1 --shell-file ${SHELL_FILE} -sASSERTIONS=1 \
		-sEXPORTED_RUNTIME_METHODS=['ccall'] -sASYNCIFY_IMPORTS=['emscripten_asm_const_int','ccall']")
else()
	target_link_libraries(${PROJECT_NAME} GLEW OSMesa)
	
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0 -Werror=return-type -DDEBUG_MODE")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -g -O2 -w")
endif()