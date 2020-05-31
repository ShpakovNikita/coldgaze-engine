#version 450

layout (set = 1, binding = 0) uniform sampler2D samplerAlbedoMap;
layout (set = 1, binding = 1) uniform sampler2D samplerNormalMap;
layout (set = 1, binding = 2) uniform sampler2D samplerOcclusionRoughnessMetallicMap;

#define LIGHTS_COUNT 6

layout (location = 0) in vec3 inWorldPos;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inUV0;
layout (location = 3) in vec2 inUV1;
layout (location = 4) in vec3 inLightPosVec[LIGHTS_COUNT];
layout (location = 10) in vec3 inLightColorVec[LIGHTS_COUNT];

// Scene bindings

layout (set = 0, binding = 0) uniform UBOScene
{
	mat4 projection;
	mat4 model;
	mat4 view;
    
    // vec4 because of alignment
    vec4 lightPos[LIGHTS_COUNT];
    vec4 lightColor[LIGHTS_COUNT];
} uboScene;

// Material bindings

layout (set = 1, binding = 0) uniform sampler2D albedoMap;
layout (set = 1, binding = 1) uniform sampler2D physicalDescriptorMap;
layout (set = 1, binding = 2) uniform sampler2D normalMap;
layout (set = 1, binding = 3) uniform sampler2D aoMap;
layout (set = 1, binding = 4) uniform sampler2D emissiveMap;

layout (push_constant) uniform Material {
	vec4 baseColorFactor;
	vec4 emissiveFactor;
	vec4 diffuseFactor;
	vec4 specularFactor;
    // int
	float workflow;
	int baseColorTextureSet;
	int physicalDescriptorTextureSet;
	int normalTextureSet;	
	int occlusionTextureSet;
	int emissiveTextureSet;
	float metallicFactor;	
	float roughnessFactor;
    // int
	float alphaMask;	
	float alphaMaskCutoff;
} material;

layout (location = 0) out vec4 outFragColor;

const float PBR_WORKFLOW_METALLIC_ROUGHNESS = 0.0;
const float PBR_WORKFLOW_SPECULAR_GLOSINESS = 1.0;

const float c_MinRoughness = 0.04;

const float DIELECTRIC_REFLECTION_APPROXIMATION = 0.04;
const float PI = 3.14159265359;

// Approximation of microfacets towards half-vector using Normal Distribution
float NDF_GGXTR(vec3 N, vec3 H, float roughness)
{
    float a = roughness*roughness;
    float a2 = a*a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;
	
    float nom = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
	
    return nom / denom;
}

// Geometry function
float G_SchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float num   = NdotV;
    float denom = NdotV * (1.0 - k) + k;
	
    return num / denom;
}

// This method covers geometry obstruction and shadowing cases
float G_Smith(vec3 N, vec3 V, vec3 L, float k)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx1 = G_SchlickGGX(NdotV, k);
    float ggx2 = G_SchlickGGX(NdotL, k);
	
    return ggx1 * ggx2;
}

// Fresnel function
vec3 F_Schlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

// Result reflection
vec3 BRDF_CookTorrance(float roughness, float metalness, vec3 albedo, vec3 N, vec3 V, vec3 F0, vec3 lightPos, vec3 lightColor)
{
    vec3 L = normalize(lightPos - inWorldPos);	
    vec3 H = normalize(V + L);
    float distance = length(lightPos - inWorldPos);
    float attenuation = 1.0 / (distance * distance);
    vec3 radiance = lightColor * attenuation;        
    
    // Cook-Torrance BRDF
    float NDF = NDF_GGXTR(N, H, roughness);        
    float G = G_Smith(N, V, L, roughness);      
    vec3 F = F_Schlick(max(dot(H, V), 0.0), F0);       
    
    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metalness;	  
    
    vec3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0);
    vec3 specular = numerator / max(denominator, 0.001);  
    
    float NdotL = max(dot(N, L), 0.0);                
    return (kD * albedo.xyz / PI + specular) * radiance * NdotL; 
}

// More info http://www.thetenthplanet.de/archives/1180
vec3 perturbNormal()
{
	vec3 tangentNormal;
    vec2 inUV;
 
    if (material.normalTextureSet > -1) {
        inUV = material.normalTextureSet == 0 ? inUV0 : inUV1;
        tangentNormal = texture(normalMap, inUV).xyz * 2.0 - 1.0;
    } else {
        return normalize(inNormal);
    }
 
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
	vec3 diffuseColor;
	vec4 albedo;

	if (material.alphaMask == 1.0f) {
		if (material.baseColorTextureSet > -1) {
			albedo = texture(albedoMap, material.baseColorTextureSet == 0 ? inUV0 : inUV1) * material.baseColorFactor;
		} else {
			albedo = material.baseColorFactor;
		}
		if (albedo.a < material.alphaMaskCutoff) {
			discard;
		}
	}
    
    vec4 localPos = uboScene.projection * uboScene.view * vec4(inWorldPos, 1.0);

    if (material.workflow == PBR_WORKFLOW_METALLIC_ROUGHNESS)
    {
        vec3 N = perturbNormal();
        vec3 V = normalize(-localPos.xyz);
        
        float occlusion = 1.0f;
        float roughness = material.roughnessFactor;
        float metallic = material.metallicFactor;
        
        if (material.physicalDescriptorTextureSet > -1) {
			// Roughness is stored in the 'g' channel, metallic is stored in the 'b' channel.
			// This layout intentionally reserves the 'r' channel for (optional) occlusion map data
			vec4 mrSample = texture(physicalDescriptorMap, material.physicalDescriptorTextureSet == 0 ? inUV0 : inUV1);
			roughness = mrSample.g * roughness;
			metallic = mrSample.b * metallic;
		} else {
			roughness = clamp(roughness, c_MinRoughness, 1.0);
			metallic = clamp(metallic, 0.0, 1.0);
		}
        
        if (material.occlusionTextureSet > -1) {
            float occlusion = texture(aoMap, (material.occlusionTextureSet == 0 ? inUV0 : inUV1)).r;
        }
        
        vec3 F0 = vec3(0.04); 
        F0 = mix(F0, albedo.xyz, metallic);
        
        vec3 radiance = vec3(0.0);
        for (int i = 0; i < LIGHTS_COUNT; ++i)
        {			
            radiance += BRDF_CookTorrance(roughness, metallic, albedo.xyz, N, V, F0, uboScene.lightPos[i].xyz, uboScene.lightColor[i].xyz);
        }
        
        vec3 ambient = vec3(0.03) * albedo.xyz * occlusion;
        vec3 diffuseColor = ambient + radiance;   
        
        // Tone compression 
        diffuseColor = diffuseColor / (diffuseColor + vec3(1.0));
        diffuseColor = pow(diffuseColor, vec3(1.0/2.2));  
        
        outFragColor = vec4(diffuseColor, albedo.a);
    }
    else
    {
        outFragColor = vec4(1.0f, 0.0f, 0.0f, 1.0f);
    }
    
    if (material.workflow == PBR_WORKFLOW_SPECULAR_GLOSINESS)
    {
        outFragColor = vec4(0.0f, 0.0f, 1.0f, 1.0f);
    }
}