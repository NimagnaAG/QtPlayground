#version 400
// GLSL version 4.0

// vertex shader
// transforms the vertex position using a camera matrix
// and interpolates the texture coordinates

// input data
layout(location = 0) in vec3 position;					// 0: vertex position with 3 elements per vertex
layout(location = 1) in vec2 imageTextureCoordinates;	// 1: image texture coordinates with 2 elements per vertex
layout(location = 2) in vec2 maskTextureCoordinates;	// 2: mask texture coordinates with 2 elements per vertex

// output to fragment shader
out vec2 interpolatedImageTextureCoordinates;			// output: computed texture coordinates
out vec2 interpolatedMaskTextureCoordinates;			// output: computed mask texture coordinates

// static
uniform mat4 worldToView;								// parameter: the camera matrix

void main() {
  // camera transformation of the vertex position
  gl_Position = worldToView * vec4(position, 1.0);
  // texture coordinate interpolation
  interpolatedImageTextureCoordinates = imageTextureCoordinates;
  interpolatedMaskTextureCoordinates = maskTextureCoordinates;
}
