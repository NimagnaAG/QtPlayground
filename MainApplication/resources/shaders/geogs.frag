#version 400 core

in vec4 gColor;
in vec2 gPosition;
in float gDepth;

uniform bool renderDepth;
out vec4 fragColor;

void main() {
    float A = -dot(gPosition, gPosition);
    if (A < -4.0) discard;
    float B = exp(A) * gColor.a;
    fragColor = vec4(B * gColor.rgb, B);

    if (renderDepth) {
        float depth_ndc = gl_FragCoord.z;
        const float u_near = 0.2; // near plane
        const float u_far = 10.0; // near plane
        float linear_depth = (2.0 * u_near) / (u_far + u_near - depth_ndc * (u_far - u_near));
        float gray = 1.0 - linear_depth;
        fragColor = vec4(gray, gray, gray, fragColor.a); 
    }    
}