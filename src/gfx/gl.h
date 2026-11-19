#pragma once

// The one place that includes the OpenGL function declarations.
//
// Desktop: glad loads the OpenGL 3.3 core functions at run time
// (gladLoadGLLoader), because on Windows and Linux only OpenGL 1.1 can be
// linked directly.
//
// Web (Emscripten): the browser provides WebGL 2, which is OpenGL ES 3.0.
// Emscripten links those functions directly, so no loader is needed.
#ifdef __EMSCRIPTEN__
#include <GLES3/gl3.h>
#define ROOMCRAFT_GLSL_VERSION "#version 300 es\nprecision highp float;\nprecision highp int;\n"
#define ROOMCRAFT_IMGUI_GLSL_VERSION "#version 300 es"
#else
#include <glad/glad.h>
#define ROOMCRAFT_GLSL_VERSION "#version 330 core\n"
#define ROOMCRAFT_IMGUI_GLSL_VERSION "#version 330"
#endif
