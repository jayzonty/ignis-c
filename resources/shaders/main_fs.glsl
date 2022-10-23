#version 460

layout (location = 0) in vec4 fragColor;
layout (location = 1) in vec2 fragUV;

layout (location = 0) out vec4 finalFragColor;

layout (binding = 0) uniform sampler2D tex;

void main()
{
    vec4 texColor = texture(tex, fragUV);
    finalFragColor = fragColor * texColor;
}
