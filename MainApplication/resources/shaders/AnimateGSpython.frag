#version 430
precision highp float;

in vec4 vColor;
in vec2 vPosition;
in float vDebugValue;

layout(location = 0) out vec4 fragColor;
layout(location = 1) out float fragDebug;

void main() {
    float A = -dot(vPosition, vPosition);
    if (A < -4.0) discard;
    float B = exp(A) * vColor.a;
    fragColor = vec4(vColor.rgb * B, B);
    fragDebug = vDebugValue;
}