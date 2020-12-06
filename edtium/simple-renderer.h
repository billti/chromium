#pragma once

#define GL_GLEXT_PROTOTYPES

// OpenGL ES includes
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

namespace CoreAppAngle {
class SimpleRenderer {
 public:
  SimpleRenderer();
  ~SimpleRenderer();
  void Draw();
  void UpdateWindowSize(GLsizei width, GLsizei height);

 private:
  GLuint mProgram;
  GLsizei mWindowWidth;
  GLsizei mWindowHeight;

  GLint mPositionAttribLocation;
  GLint mColorAttribLocation;

  GLint mModelUniformLocation;
  GLint mViewUniformLocation;
  GLint mProjUniformLocation;

  GLuint mVertexPositionBuffer;
  GLuint mVertexColorBuffer;
  GLuint mIndexBuffer;

  int mDrawCount;
};
}  // namespace CoreAppAngle
