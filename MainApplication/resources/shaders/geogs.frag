#version 400 core
in vec4 gColor;
in vec2 gPosition;
out vec4 fragColor;

void main() {
    float A = -dot(gPosition, gPosition);
    if (A < -4.0) discard;
    float B = exp(A) * gColor.a;
    fragColor = vec4(B * gColor.rgb, B);
}