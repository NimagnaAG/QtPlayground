#version 400
precision highp float;
precision highp int;

layout(binding = 0) uniform usampler2D uTexture;
uniform mat4 uProj;
uniform mat4 uView;
uniform vec2 uFocal;     // focal in pixel eg [1150, 1150]
uniform vec2 uViewport;  // resolution in pixel eg [1920, 1080]

layout(location = 0) in vec2 aPosition;
layout(location = 1) in int aIndex;

out vec4 vColor;
out vec2 vPosition;

void main() {
    // Extract x and y coordinates for texture lookup
    uint x = (uint(aIndex) & 0x3ffu) << 1;  // Extract lower 10 bits and multiply by 2
    uint y = uint(aIndex) >> 10;            // Extract upper bits

    uvec4 centeru = texelFetch(uTexture, ivec2(x, y), 0);
    vec4 center = vec4(uintBitsToFloat(centeru.xyz), 1);
    vec4 cam = uView * center;
    vec4 pos2d = uProj * cam;

    float clip = 1.2 * pos2d.w;
    if (pos2d.z < -clip || pos2d.x < -clip || pos2d.x > clip ||
        pos2d.y < -clip || pos2d.y > clip) {
        gl_Position = vec4(0.0, 0.0, 2.0, 1.0);
        return;
    }

    uvec4 cov = texelFetch(uTexture, ivec2(x | 1u, y), 0);
    vec2 u1 = unpackHalf2x16(cov.x);
    vec2 u2 = unpackHalf2x16(cov.y);
    vec2 u3 = unpackHalf2x16(cov.z);
    mat3 Vrk = 4.0 * mat3(
        u1.x, u1.y, u2.x,
        u1.y, u2.y, u3.x,
        u2.x, u3.x, u3.y
    );

    mat3 J = mat3(
        uFocal.x / cam.z, 0., -(uFocal.x * cam.x) / (cam.z * cam.z),
        0., -uFocal.y / cam.z, (uFocal.y * cam.y) / (cam.z * cam.z),
        0., 0., 0.
    );

    mat3 T = transpose(mat3(uView)) * J;
    mat3 cov2d = transpose(T) * Vrk * T;

    float mid = (cov2d[0][0] + cov2d[1][1]) / 2.0;
    float radius = length(vec2((cov2d[0][0] - cov2d[1][1]) / 2.0, cov2d[0][1]));
    float lambda1 = mid + radius;
    float lambda2 = mid - radius;

    if(lambda2 < 0.0) {
        gl_Position = vec4(0.0, 0.0, 2.0, 1.0);
        return;
    }

    vec2 diagonalVector = normalize(vec2(cov2d[0][1], lambda1 - cov2d[0][0]));
    vec2 majorAxis = min(sqrt(2.0 * lambda1), 1024.0) * diagonalVector;
    vec2 minorAxis = min(sqrt(2.0 * lambda2), 1024.0) *
                    vec2(diagonalVector.y, -diagonalVector.x);

    vColor = clamp(pos2d.z/pos2d.w+1.0, 0.0, 1.0) * vec4(
        float(cov.w & 0xffu),
        float((cov.w >> 8) & 0xffu),
        float((cov.w >> 16) & 0xffu),
        float((cov.w >> 24) & 0xffu)
    ) / 255.0;

    vPosition = aPosition;
    vec2 vCenter = vec2(pos2d) / pos2d.w;

    gl_Position = vec4(
        vCenter + (aPosition.x * majorAxis + aPosition.y * minorAxis) / uViewport,
        0.0,
        1.0
    );
}