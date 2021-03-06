#version 460
#extension GL_NV_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable

struct RayPayload
{
	vec3 color;
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
	float workflow;
	int baseColorTextureSet;
	int physicalDescriptorTextureSet;
	int normalTextureSet;	
	int occlusionTextureSet;
	int emissiveTextureSet;
	float metallicFactor;	
	float roughnessFactor;
	float alphaMask;	
	float alphaMaskCutoff;
};

struct PBRParams {
    float NdotL;                  // cos angle between normal and light direction
	float NdotV;                  // cos angle between normal and view direction
	float NdotH;                  // cos angle between normal and half vector
	float LdotH;                  // cos angle between light direction and half vector
	float VdotH;                  // cos angle between view direction and half vector
    float occlusion;              // pre calculated occlusion value on the surface
	float roughness;              // roughness value at the surface
	float metallic;               // metallic value at the surface
	float alphaRoughness;         // roughness * roughness
	vec3 diffuseColor;            // color contribution from diffuse lighting
	vec3 specularColor;           // color contribution from specular lighting
	vec4 albedo;                  // base albedo color value
	vec3 worldPos;                // point on surface world position
    vec3 N;                       // World normal
};

layout(binding = 2, set = 0) uniform UBOScene
{
	mat4 projection;
	mat4 model;
	mat4 view;
    vec4 cameraPos;
    
    mat4 invProjection;
	mat4 invView;
    
    vec4 globalLightDir;
    vec4 globalLightColor;
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
float NDF_GGXTR(PBRParams pbrParams)
{
    float a      = pbrParams.alphaRoughness;
    float a2     = a * a;
    float NdotH2 = pbrParams.NdotH * pbrParams.NdotH;
	
    float num   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
	
    return num / denom;
}

// Geometry function
float G_SchlickGGX(float cosTheta, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    float num   = cosTheta;
    float denom = cosTheta * (1.0 - k) + k;
	
    return num / denom;
}

// This method covers geometry obstruction and shadowing cases
float G_Smith(PBRParams pbrParams)
{
    float ggx1 = G_SchlickGGX(pbrParams.NdotV, pbrParams.roughness);
    float ggx2 = G_SchlickGGX(pbrParams.NdotL, pbrParams.roughness);
	
    return ggx1 * ggx2;
}

// Fresnel function
vec3 F_Schlick(PBRParams pbrParams)
{
    return pbrParams.specularColor + (1.0 - pbrParams.specularColor) * pow(clamp(1.0 - pbrParams.VdotH, 0.0, 1.0), 5.0);
}

// Result reflection
vec3 BRDF_CookTorrance(vec3 lightColor, VertexData vertexData, PBRParams pbrParams)
{
    // Cook-Torrance BRDF
    float NDF = NDF_GGXTR(pbrParams);        
    float G = G_Smith(pbrParams);      
    vec3 F = F_Schlick(pbrParams);
    
    vec3 num = NDF * G * F;
    float denom = 4.0 * pbrParams.NdotV * pbrParams.NdotL;
    vec3 specularContrib = num / max(denom, 0.001);  
    
    return specularContrib;
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
vec3 perturbNormal(VertexData vertexData, Material material, vec3 worldPos)
{
    vec3 worldNormal = normalize(gl_ObjectToWorldNV * vertexData.inNormal);
    
	vec3 tangentNormal;
    vec2 inUV;
 
    if (material.normalTextureSet > -1) {
        inUV = material.normalTextureSet == 0 ? vertexData.inUV.xy : vertexData.inUV.zw;
        tangentNormal = texture(normalTextures[nonuniformEXT(gl_InstanceCustomIndexNV)], inUV).xyz * 2.0 - 1.0;
    } else {
        return worldNormal;
    }

	vec3 q1 = dFdx(worldPos);
	vec3 q2 = dFdy(worldPos);
	vec2 st1 = dFdx(inUV);
	vec2 st2 = dFdy(inUV);

	vec3 T = normalize(q1 * st2.t - q2 * st1.t);
    vec3 N = normalize(worldNormal);
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

PBRParams GetPBRParams(VertexData vertexData, Material material)
{
    vec4 albedo;
    if (material.baseColorTextureSet > -1) {
        albedo = texture(baseColorTextures[nonuniformEXT(gl_InstanceCustomIndexNV)], 
            material.baseColorTextureSet == 0 ? vertexData.inUV.xy :  vertexData.inUV.zw);
    } else {
        albedo = vec4(1.0); // material.baseColorFactor;
    }

    vec3 worldPos = gl_WorldRayOriginNV + gl_HitTNV * gl_WorldRayDirectionNV;
    
    vec3 N = perturbNormal(vertexData, material, worldPos);
    vec3 V = normalize(gl_WorldRayOriginNV - worldPos);
    vec3 L = normalize(uboScene.globalLightDir.xyz);	
    vec3 H = normalize(V + L);
    
    float occlusion = 1.0f;
    float roughness = material.roughnessFactor;
    float metallic = material.metallicFactor;
    
    if (material.physicalDescriptorTextureSet > -1) {
        vec4 mrSample = texture(physicalDescriptorTextures[nonuniformEXT(gl_InstanceCustomIndexNV)], material.physicalDescriptorTextureSet == 0 ? vertexData.inUV.xy : vertexData.inUV.zw);
        roughness = mrSample.g * roughness;
        metallic = mrSample.b * metallic;
    } else {
        roughness = clamp(roughness, c_MinRoughness, 1.0);
        metallic = clamp(metallic, 0.0, 1.0);
    }
    
    if (material.occlusionTextureSet > -1) {
        occlusion = texture(ambientOcclusionTextures[nonuniformEXT(gl_InstanceCustomIndexNV)], (material.occlusionTextureSet == 0 ? vertexData.inUV.xy : vertexData.inUV.zw)).r;
    }
    
    vec3 F0 = vec3(0.04); 
    vec3 specularColor = mix(F0, albedo.rgb, metallic);
    
    vec3 diffuseColor = albedo.rgb * (vec3(1.0) - F0);
    diffuseColor *= 1.0 - metallic;
    
    float NdotL = clamp(dot(N, L), 0.001, 1.0);
    float NdotV = clamp(abs(dot(N, V)), 0.001, 1.0);
    float NdotH = clamp(dot(N, H), 0.0, 1.0);
    float LdotH = clamp(dot(L, H), 0.0, 1.0);
    float VdotH = clamp(dot(V, H), 0.0, 1.0);
    
    PBRParams pbrParams = PBRParams(
        NdotL,
        NdotV,
        NdotH,
        LdotH,
        VdotH,
        occlusion,
        roughness,
        metallic,
        roughness * roughness,
        diffuseColor,
        specularColor,
        albedo,
        worldPos,
        N
    );
    
    return pbrParams;
}

void main()
{
    const vec3 barycentrics = vec3(1.0f - attribs.x - attribs.y, attribs.x, attribs.y);
      
    const VertexData v0 = FetchVertexData(0);
    const VertexData v1 = FetchVertexData(1);
    const VertexData v2 = FetchVertexData(2);
    
    const VertexData vertexData = BaryLerp(v0, v1, v2, barycentrics);
    const Material material = materialBuffers[nonuniformEXT(gl_InstanceCustomIndexNV)].material;
    
    if (material.workflow == PBR_WORKFLOW_METALLIC_ROUGHNESS)
    {
        PBRParams pbrParams = GetPBRParams(vertexData, material);
        
        vec3 specularContrib = BRDF_CookTorrance(uboScene.globalLightColor.rgb, vertexData, pbrParams);
        vec3 diffuseContrib = pbrParams.diffuseColor / PI;
        
        rayPayload.color = (specularContrib + diffuseContrib) * pbrParams.NdotL * uboScene.globalLightColor.rgb * pbrParams.occlusion;
    }
    else
    {
        rayPayload.color = vec3(1.0, 0.0, 0.0);
    }
}
