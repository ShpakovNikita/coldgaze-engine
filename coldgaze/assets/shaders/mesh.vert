#version 450

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inUV;
layout (location = 3) in vec3 inColor;

#define lightCount 6

layout (set = 0, binding = 0) uniform UBOScene
{
	mat4 projection;
	mat4 view;
    
    // vec4 because of alignment
    vec4 lightPos[lightCount];
    vec4 lightColor[lightCount];
} uboScene;

layout(push_constant) uniform PushConsts {
	mat4 model;
} primitive;

layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec3 outColor;
layout (location = 2) out vec2 outUV;
layout (location = 3) out vec3 outViewVec;
layout (location = 4) out vec3 outWorldPos;
layout (location = 5) out vec3 outLightPosVec[lightCount];
layout (location = 11) out vec3 outLightColorVec[lightCount];


void main() 
{
	outNormal = inNormal;
	outColor = inColor;
	outUV = inUV;
	gl_Position = uboScene.projection * uboScene.view * primitive.model * vec4(inPos.xyz, 1.0);
	
	vec4 pos = uboScene.view * vec4(inPos, 1.0);
	outNormal = mat3(uboScene.view) * inNormal;
	outViewVec = -pos.xyz;		
    outWorldPos = inPos;
    
    for (int i = 0; i < lightCount; ++i)
	{	
		outLightPosVec[i] = mat3(uboScene.view) * uboScene.lightPos[i].xyz;
		outLightColorVec[i] = uboScene.lightColor[i].xyz;
	}
}