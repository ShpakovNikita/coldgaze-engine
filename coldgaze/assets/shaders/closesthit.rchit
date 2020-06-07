#version 460
#extension GL_NV_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable

#define LIGHTS_COUNT 6

struct RayPayload
{
	vec4 colorAndDistance; // rgb + t
	vec4 scatterDirection; // xyz + w (is scatter needed)
	uint randomSeed;
};

struct VertexData
{
    vec4 inPos;
    vec4 inNormal;
    vec4 inUV; // xy = UV0, zw = UV1 
    // inJoint0;
    // inWeight0;
};

struct Material {
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
};

layout(binding = 2, set = 0) uniform UBOScene
{
	mat4 projection;
	mat4 model;
	mat4 view;
    vec4 cameraPos;
    
    // vec4 because of alignment
    vec4 lightPos[LIGHTS_COUNT];
    vec4 lightColor[LIGHTS_COUNT];
} uboScene;

layout(binding = 0, set = 1) readonly buffer VertexBuffers { VertexData vertices[]; } vertexBuffers[];
layout(binding = 1, set = 1) readonly buffer IndexBuffers { uint indices[]; } indexBuffers[];
layout(binding = 2, set = 1) uniform MaterialBuffers { Material material; } materialBuffers[];
layout(binding = 3, set = 1) uniform sampler2D baseColorTextures[];
layout(binding = 4, set = 1) uniform sampler2D physicalDescriptorTextures[];
layout(binding = 5, set = 1) uniform sampler2D normalTextures[];
layout(binding = 6, set = 1) uniform sampler2D ambientOcclusionTextures[];
layout(binding = 7, set = 1) uniform sampler2D emissiveTextures[];

layout(binding = 0, set = 2) uniform sampler2D equirectangularMap;

layout(location = 0) rayPayloadInNV RayPayload rayPayload;
hitAttributeNV vec3 attribs;

const float PBR_WORKFLOW_METALLIC_ROUGHNESS = 0.0;
const float PBR_WORKFLOW_SPECULAR_GLOSINESS = 1.0;

const float c_MinRoughness = 0.04;

const float DIELECTRIC_REFLECTION_APPROXIMATION = 0.04;
const float PI = 3.14159265359;

// Approximation of microfacets towards half-vector using Normal Distribution
float NDF_GGXTR(vec3 N, vec3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
	
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
vec3 BRDF_CookTorrance(float roughness, float metallic, vec3 albedo, vec3 N, vec3 V, vec3 F0, vec3 lightPos, vec3 lightColor, VertexData vertexData)
{
    vec3 L = normalize(vertexData.inPos.xyz - lightPos);	
    vec3 H = normalize(V + L);
    float distance = length(lightPos - vertexData.inPos.xyz);
    float attenuation = 1.0 / (distance * distance);
    vec3 radiance = lightColor * attenuation;        
    
    // Cook-Torrance BRDF
    float NDF = NDF_GGXTR(N, H, roughness);        
    float G = G_Smith(N, V, L, roughness);      
    vec3 F = F_Schlick(max(dot(H, V), 0.0), F0);       
    
    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metallic;	  
    
    vec3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0);
    vec3 specular = numerator / max(denominator, 0.001);  
    
    float NdotL = max(dot(N, L), 0.0);                
    return (kD * albedo.xyz / PI + specular) * radiance * NdotL; 
}

vec2 dFdy(vec2 p)
{
    return vec2(p.x, p.y + 0.01) - p;
}

vec3 dFdy(vec3 p)
{
    return vec3(p.x, p.y + 0.01, p.z) - p;
}

vec2 dFdx(vec2 p)
{
    return vec2(p.x + 0.01, p.y) - p;
}

vec3 dFdx(vec3 p)
{
    return vec3(p.x + 0.01, p.yz) - p;
}

// More info http://www.thetenthplanet.de/archives/1180
vec3 perturbNormal(VertexData vertexData, Material material)
{
	vec3 tangentNormal;
    vec2 inUV;
 
    if (material.normalTextureSet > -1) {
        inUV = material.normalTextureSet == 0 ? vertexData.inUV.xy : vertexData.inUV.zw;
        tangentNormal = texture(normalTextures[nonuniformEXT(gl_InstanceCustomIndexNV)], inUV).xyz * 2.0 - 1.0;
    } else {
        return normalize(vertexData.inNormal.xyz);
    }

	vec3 q1 = dFdx(vertexData.inPos.xyz);
	vec3 q2 = dFdy(vertexData.inPos.xyz);
	vec2 st1 = dFdx(inUV);
	vec2 st2 = dFdy(inUV);

	vec3 T = normalize(q1 * st2.t - q2 * st1.t);
    vec3 N = normalize(vertexData.inNormal.xyz);
	vec3 B = -normalize(cross(N, T));
	mat3 TBN = mat3(T, B, N);

	return normalize(TBN * tangentNormal);
}

vec2 BaryLerp(vec2 a, vec2 b, vec2 c, vec3 barycentrics)
{
    return a * barycentrics.x + b * barycentrics.y + c * barycentrics.z;
}

vec3 BaryLerp(vec3 a, vec3 b, vec3 c, vec3 barycentrics)
{
    return a * barycentrics.x + b * barycentrics.y + c * barycentrics.z;
}

vec4 BaryLerp(vec4 a, vec4 b, vec4 c, vec3 barycentrics)
{
    return a * barycentrics.x + b * barycentrics.y + c * barycentrics.z;
}

VertexData BaryLerp(VertexData v0, VertexData v1, VertexData v2, vec3 barycentrics)
{
    VertexData outData;
    outData.inUV = BaryLerp(v0.inUV, v1.inUV, v2.inUV, barycentrics);
    outData.inPos = BaryLerp(v0.inPos, v1.inPos, v2.inPos, barycentrics);
    outData.inNormal = BaryLerp(v0.inNormal, v1.inNormal, v2.inNormal, barycentrics);
    
    return outData;
}

VertexData FetchVertexData(uint offset)
{
    const uint index = indexBuffers[nonuniformEXT(gl_InstanceCustomIndexNV)].indices[gl_PrimitiveID * 3 + offset];
    return vertexBuffers[nonuniformEXT(gl_InstanceCustomIndexNV)].vertices[index];
}

void main()
{
    const vec3 barycentrics = vec3(1.0f - attribs.x - attribs.y, attribs.x, attribs.y);
      
    const VertexData v0 = FetchVertexData(0);
    const VertexData v1 = FetchVertexData(1);
    const VertexData v2 = FetchVertexData(2);
    
    const VertexData vertexData = BaryLerp(v0, v1, v2, barycentrics);
    const Material material = materialBuffers[nonuniformEXT(gl_InstanceCustomIndexNV)].material;
    
    vec4 albedo;
    if (material.baseColorTextureSet > -1) {
        albedo = texture(baseColorTextures[nonuniformEXT(gl_InstanceCustomIndexNV)], 
            material.baseColorTextureSet == 0 ? vertexData.inUV.xy :  vertexData.inUV.zw) * material.baseColorFactor;
    } else {
        albedo = material.baseColorFactor;
    }
    
    if (material.workflow == PBR_WORKFLOW_METALLIC_ROUGHNESS)
    {
        vec3 N = perturbNormal(vertexData, material);
        vec3 V = normalize(uboScene.cameraPos.xyz - vertexData.inPos.xyz);
        
        float occlusion = 1.0f;
        float roughness = material.roughnessFactor;
        float metallic = material.metallicFactor;
        
        if (material.physicalDescriptorTextureSet > -1) {
			// Roughness is stored in the 'g' channel, metallic is stored in the 'b' channel.
			// This layout intentionally reserves the 'r' channel for (optional) occlusion map data
			vec4 mrSample = texture(physicalDescriptorTextures[nonuniformEXT(gl_InstanceCustomIndexNV)], material.physicalDescriptorTextureSet == 0 ? vertexData.inUV.xy : vertexData.inUV.zw);
			roughness = mrSample.g * roughness;
			metallic = mrSample.b * metallic;
		} else {
			roughness = clamp(roughness, c_MinRoughness, 1.0);
			metallic = clamp(metallic, 0.0, 1.0);
		}
        
        if (material.occlusionTextureSet > -1) {
            float occlusion = texture(ambientOcclusionTextures[nonuniformEXT(gl_InstanceCustomIndexNV)], (material.occlusionTextureSet == 0 ? vertexData.inUV.xy : vertexData.inUV.zw)).r;
        }
        
        vec3 F0 = vec3(0.04); 
        F0 = mix(F0, albedo.xyz, metallic);
        
        vec3 radiance = vec3(0.0);
        for (int i = 0; i < LIGHTS_COUNT; ++i)
        {			
            radiance += BRDF_CookTorrance(roughness, metallic, albedo.xyz, N, V, F0, uboScene.lightPos[i].xyz, uboScene.lightColor[i].xyz, vertexData);
        }
        
        vec3 ambient = vec3(0.03) * albedo.xyz * occlusion;
        vec3 diffuseColor = ambient + radiance;   
        
        // Tone compression 
        diffuseColor = diffuseColor / (diffuseColor + vec3(1.0));
        diffuseColor = pow(diffuseColor, vec3(1.0/2.2));  
        
        rayPayload.colorAndDistance.xyz = diffuseColor;
    }
    else
    {
        rayPayload.colorAndDistance.xyz = vec3(1.0, 0.0, 0.0);
    }
}
