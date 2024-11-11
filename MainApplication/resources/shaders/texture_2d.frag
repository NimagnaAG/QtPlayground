#version 400 core

// fragment shader
// calculates the pixel color using a texture 
// and the transparency using a mask texture with separate texture coordinates

// inputs
in vec2 interpolatedImageTextureCoordinates;	// input: texture coordinate (xy-coordinates)
in vec2 interpolatedMaskTextureCoordinates;		// input: mask texture coordinate (xy-coordinates)

// outputs
out vec4 finalColor;							// output: final color value as rgba-value

// static input: textures
uniform sampler2D imageTexture;			        // the rectangular image texture
uniform sampler2D maskTexture;			        // the rectangular mask texture (key)

// general
uniform bool useMaskTexture;                    // use the separate mask texture instead of the image's alpha channel
uniform bool swapRGB;                           // swap RGB to BGR (or vice versa)
uniform float alphaTransparency;                // alpha transparency multiplied on top

// post processing
uniform bool doBlurring;                        // apply blurring or not on the alpha channel
uniform bool isPostProcessingEnabled;           // use post processing
uniform int blurKernelSize;                     // the blurring kernel size
uniform float sharpnessValue;                   // sharpness of the sigmoid filter

// get a sigmoid function value
float sigmoidFilter(float value) {
  float sigSlope = sharpnessValue;
  float sig = exp(sigSlope * (value - 0.5f));
  return sig / (1.0f + sig);
}

// calculate average blur around pixel using 2D texture
float averageBlur(int radius) {
  int diameter = 2*radius + 1;
  float sampleBlurred = 0.0f;
  ivec2 textureSize = textureSize(maskTexture,0);
  float stepSizeX = 1.0f/float(textureSize.x);
  float stepSizeY = 1.0f/float(textureSize.y);
  for(int i = -radius; i <= radius; i++) {
    for(int j = -radius; j <= radius; j++) {
      if (isPostProcessingEnabled) {
        sampleBlurred += sigmoidFilter(texture(maskTexture, interpolatedMaskTextureCoordinates + vec2(i * stepSizeX, j * stepSizeY)).r);
      } else {
        sampleBlurred += texture(maskTexture, interpolatedMaskTextureCoordinates + vec2(i * stepSizeX, j * stepSizeY)).r;
      }
    }
  }
  return sampleBlurred / (diameter * diameter * 1.0f);
}

void main() {
  // use a separate texture for the mask/alpha channel?
  if (useMaskTexture) {
    // Use RGB from image texture and separate Alpha texture for transparency
    // use 2D texture target!
    finalColor.rgb = texture(imageTexture, interpolatedImageTextureCoordinates).rgb;

    // for the alpha channel, there are different options:
    if (useMaskTexture && doBlurring) {
    // blur the alpha mask
    float sampleBlurred = averageBlur(blurKernelSize);
    finalColor.a = smoothstep(0.0f, 1.0f, sampleBlurred);
    } else {
    // just use the mask texture
    finalColor.a = texture(maskTexture, interpolatedMaskTextureCoordinates).r;
    }
  } else {
    // no mask texture -> use RGBA from image texture
    finalColor.rgba = texture(imageTexture, interpolatedImageTextureCoordinates).rgba;
  }

  if (swapRGB) {
    // swap R and B channel
    finalColor.rgba = finalColor.bgra;
  }

  // apply alpha transparency
  finalColor.a = finalColor.a * alphaTransparency;
}
