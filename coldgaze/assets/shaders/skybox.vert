#version 450

layout (location = 0) in vec3 inPos;

layout (set = 0, binding = 0) uniform UBOScene
{
	mat4 projection;
	mat4 view;
} uboScene;

layout (location = 0) out vec3 outWorldPos;

void main() 
{
    // discard position data 
    mat4 rotView = mat4(mat3(uboScene.view));
	gl_Position = uboScene.projection * rotView * vec4(inPos.xyz, 1.0);
    outWorldPos = inPos;
}