#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoords;
in vec4 ClipSpacePos; // Receive gl_Position from vertex shader

uniform sampler2D texture_diffuse1;
uniform sampler2D textures[64];  // Maximum of 16 textures
uniform int numTextures;


uniform vec3 lightPos;
uniform vec3 viewPos;
uniform vec3 lightColor;
uniform vec3 objectColor;

void main()
{
    // Sample the texture
    vec4 texColor = texture(texture_diffuse1, TexCoords);

    // Set the fragment color
    FragColor = texColor;
}