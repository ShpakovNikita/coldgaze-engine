#version 450

layout (set = 1, binding = 0) uniform sampler2D samplerColorMap;
layout (set = 1, binding = 1) uniform sampler2D samplerNormalMap;
layout (set = 1, binding = 2) uniform sampler2D samplerOcclusionRoughnessMetallicMap;

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec3 inColor;
layout (location = 2) in vec2 inUV;
layout (location = 3) in vec3 inViewVec;
layout (location = 4) in vec3 inLightVec;
layout (location = 5) in vec3 inWorldPos;

layout (location = 0) out vec4 outFragColor;

// More info http://www.thetenthplanet.de/archives/1180
vec3 perturbNormal()
{
	vec3 tangentNormal = texture(samplerNormalMap, inUV).xyz * 2.0 - 1.0;
    
	vec3 q1 = dFdx(inWorldPos);
	vec3 q2 = dFdy(inWorldPos);
	vec2 st1 = dFdx(inUV);
	vec2 st2 = dFdy(inUV);

	vec3 T = normalize(q1 * st2.t - q2 * st1.t);
    vec3 N = normalize(inNormal);
	vec3 B = -normalize(cross(N, T));
	mat3 TBN = mat3(T, B, N);

	return normalize(TBN * tangentNormal);
}

void main() 
{
	vec4 color = texture(samplerColorMap, inUV) * vec4(inColor, 1.0);

	vec3 N = perturbNormal();
	vec3 L = normalize(inLightVec);
	vec3 V = normalize(inViewVec);
    vec3 R = reflect(L, N);
    vec3 ambient = color.rgb * vec3(0.5);
	vec3 diffuse = dot(N, -L) * 0.3 * inColor;
	vec3 specular = pow(max(dot(R, V), 0.0), 16.0) * vec3(0.75);
	outFragColor = vec4(ambient + diffuse + specular, 1.0);	
}