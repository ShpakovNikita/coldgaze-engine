#version 460
#extension GL_NV_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable

struct RayPayload
{
	vec3 color;
	vec3 throughput;
    uint bouncesCount;
	uint randomSeed;
    uint envHit;
    
    // dirty hack
    uint sampleEnviroment;
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
	vec4 albedo;                  // base albedo color value
	vec3 emissive;                // emissive color value
	vec3 worldPos;                // point on surface world position
    vec3 F0;                      // F0 param for Fresnel function
    vec3 V;                       // View vector
    vec3 N;                       // Normal vector
    vec3 H;                       // Half vector
    vec3 L;                       // Liight vector
    float sw;                     // Specular weight determine, how we are going to launch ray
};

layout(binding = 0, set = 0) uniform accelerationStructureNV topLevelAS;
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

layout(binding = 4, set = 0) uniform Camera
{
    uint bouncesCount;
    uint numberOfSamples;
    float aperture;
    float focusDistance;
    uint pauseRendering;
    uint accumulationIndex;
    uint randomSeed;
} camera;


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
layout(location = 1) rayPayloadNV RayPayload indirect;
layout(location = 2) rayPayloadNV RayPayload direct;

hitAttributeNV vec3 attribs;

const float EPSILON = 0.001;

const float PBR_WORKFLOW_METALLIC_ROUGHNESS = 0.0;
const float PBR_WORKFLOW_SPECULAR_GLOSINESS = 1.0;

const float c_MinRoughness = 0.04;

const float DIELECTRIC_REFLECTION_APPROXIMATION = 0.04;
const float MIN_TERMINATION_THRESHOLD = 0.05;
const float PI = 3.14159265359;

const float RAY_MIN = 0.001;
const float RAY_MAX = 10000.0;

// Needed for specular weight, https://github.com/Nadrin/Quartz/blob/master/src/raytrace/renderers/vulkan/shaders/lib/common.glsl
float Luminance(vec3 color)
{
    return dot(color, vec3(0.2126, 0.7152, 0.0722));
}

float GetSpecularWeight(vec3 baseColor, vec3 F0, float metallic)
{
    const float diffuseLum = mix(Luminance(baseColor), 0, metallic);
    const float specularLum = Luminance(F0);
    return min(1, specularLum / (specularLum + diffuseLum));
}

bool IsBlack(vec3 color)
{
    return dot(color, color) < EPSILON;
}

uint RandomInt(inout uint seed)
{
	// LCG values from Numerical Recipes
    return (seed = 1664525 * seed + 1013904223);
}

float RandomFloat(inout uint seed)
{
	// Float version using bitmask from Numerical Recipes
	const uint one = 0x3f800000;
	const uint msk = 0x007fffff;
	return uintBitsToFloat(one | (msk & (RandomInt(seed) >> 9))) - 1;
}

vec2 RandomInUnitDisk(inout uint seed)
{
	for (;;)
	{
		const vec2 p = 2 * vec2(RandomFloat(seed), RandomFloat(seed)) - 1;
		if (dot(p, p) < 1)
		{
			return p;
		}
	}
}

vec3 RandomInUnitSphere(inout uint seed)
{
	for (;;)
	{
		const vec3 p = 2 * vec3(RandomFloat(seed), RandomFloat(seed), RandomFloat(seed)) - 1;
		if (dot(p, p) < 1)
		{
			return p;
		}
	}
}

vec2 SampleDisk(vec2 u)
{
    float r = sqrt(u.x);
    float theta = 2 * PI * u.y;
    return r * vec2(cos(theta), sin(theta));
}

vec3 SampleHemisphereCosine(vec2 u)
{
    vec2 d = SampleDisk(u);
    return vec3(d.x, d.y, sqrt(1.0 - d.x * d.x - d.y * d.y));
}

float PdfHemisphereCosine(float cosTheta)
{
    return cosTheta / PI;
}

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
    return pbrParams.F0 + (1.0 - pbrParams.F0) * pow(1.0 - pbrParams.VdotH, 5.0);
}

vec3 SampleD_GGX(vec2 u, float alpha2)
{
    float phi    = 2 * PI * u.x;
    float cos_wh = sqrt((1.0 - u.y) / (1.0 + (alpha2 - 1.0) * u.y));
    float sin_wh = sqrt(1.0 - cos_wh * cos_wh);
    return vec3(
        sin_wh * cos(phi),
        sin_wh * sin(phi),
        cos_wh
    );
}

float PDF_D_GGX(float roughness, float cosTheta)
{
    return G_SchlickGGX(cosTheta, roughness) * cosTheta;
}


float PDF_BSDF(PBRParams pbrParams)
{
    float pdfDiffuse = PdfHemisphereCosine(pbrParams.NdotL);
    float pdfSpecular = PDF_D_GGX(pbrParams.roughness, pbrParams.NdotH) / max(EPSILON, 4.0 * pbrParams.LdotH);
    return mix(pdfDiffuse, pdfSpecular, pbrParams.sw);
}

void InitPBRParams(inout PBRParams pbrParams, vec3 N, vec3 V, vec3 L, vec3 H)
{
    pbrParams.NdotL = clamp(dot(N, L), 0.001, 1.0);
    pbrParams.NdotV = clamp(abs(dot(N, V)), 0.001, 1.0);
    pbrParams.NdotH = clamp(dot(N, H), 0.001, 1.0);
    pbrParams.LdotH = clamp(dot(L, H), 0.001, 1.0);
    pbrParams.VdotH = clamp(dot(V, H), 0.001, 1.0);
}

vec3 EvaluateBSDF(VertexData vertexData, PBRParams pbrParams)
{
    // Cook-Torrance BRDF
    float NDF = NDF_GGXTR(pbrParams);        
    float G = G_Smith(pbrParams);      
    vec3 F = F_Schlick(pbrParams);
    
    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - pbrParams.metallic;	 
    
    vec3 specular = NDF * G * F;
    vec3 diffuse = kD * pbrParams.albedo.rgb / PI;
    
    return specular + diffuse;
}

vec3 SampleBSDF(VertexData vertexData, inout PBRParams pbrParams, out float pdf)
{
    vec3 unit = RandomInUnitSphere(rayPayload.randomSeed);

    if(unit.z < pbrParams.sw) {
        pbrParams.H = SampleD_GGX(unit.xy, pbrParams.alphaRoughness * pbrParams.alphaRoughness);
        pbrParams.L = -reflect(pbrParams.V, pbrParams.H);
    }
    else {
        pbrParams.L = SampleHemisphereCosine(unit.xy);
        pbrParams.H = normalize(pbrParams.L + pbrParams.V);
    }

    InitPBRParams(pbrParams, pbrParams.N, pbrParams.V, pbrParams.L, pbrParams.H);
    
    pdf = PDF_BSDF(pbrParams);
    return EvaluateBSDF(vertexData, pbrParams);
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
            material.baseColorTextureSet == 0 ? vertexData.inUV.xy :  vertexData.inUV.zw) * material.baseColorFactor;
    } else {
        albedo = material.baseColorFactor;
    }

    vec3 emissive;
    if (material.emissiveTextureSet > -1) {
        emissive = texture(emissiveTextures[nonuniformEXT(gl_InstanceCustomIndexNV)], 
            material.emissiveTextureSet == 0 ? vertexData.inUV.xy :  vertexData.inUV.zw).rgb * material.emissiveFactor.rgb;
    } else {
        emissive = material.emissiveFactor.rgb;
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
    
    vec3 F0 = mix(vec3(DIELECTRIC_REFLECTION_APPROXIMATION), albedo.rgb, metallic);
    
    float sw = GetSpecularWeight(albedo.rgb, F0, metallic);
    
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
        albedo,
        emissive,
        worldPos,
        F0,
        V,
        N,
        H,
        L,
        sw
    );
    
    return pbrParams;
}

vec3 GetDirectLighting(PBRParams pbrParams, VertexData vertexData)
{
    uint rayFlags = gl_RayFlagsOpaqueNV | gl_RayFlagsTerminateOnFirstHitNV | gl_RayFlagsSkipClosestHitShaderNV;
    uint cullMask = 0xff;

    const float DISPERSION_FACTOR = 0.08;
    direct.envHit = 0;
    
    traceNV(topLevelAS, rayFlags, cullMask, 0, 0, 0, pbrParams.worldPos, RAY_MIN, uboScene.globalLightDir.xyz + DISPERSION_FACTOR * RandomInUnitSphere(rayPayload.randomSeed), RAY_MAX, 2);
    
    if (direct.envHit > 0)
    {
        const vec3 bsdf = EvaluateBSDF(vertexData, pbrParams);
    
        return uboScene.globalLightColor.rgb * bsdf * pbrParams.NdotL;
    }
    else
    {
        return vec3(0.0, 0.0, 0.0);
    }
    
}

vec3 GetReflections(PBRParams pbrParams)
{
    vec3 reflectance = reflect(-pbrParams.V, pbrParams.N) + pbrParams.roughness * RandomInUnitSphere(rayPayload.randomSeed);

    indirect.randomSeed = rayPayload.randomSeed;
    indirect.bouncesCount = rayPayload.bouncesCount + 1;
    indirect.sampleEnviroment = 1;
    
    uint rayFlags = gl_RayFlagsOpaqueNV;
    uint cullMask = 0xff;
    
    traceNV(topLevelAS, rayFlags, cullMask, 0, 0, 0, pbrParams.worldPos, RAY_MIN, reflectance, RAY_MAX, 1);
    
    return indirect.color * 0.1;
}

vec3 GetEnviromentLighting(PBRParams pbrParams, VertexData vertexData)
{
    return vec3(0.0);
}

float Maxcomp(vec3 comps)
{
    return max(comps.x, max(comps.y, comps.z));
}

vec3 GetIndirectLighting(PBRParams pbrParams, VertexData vertexData, uint minDepth)
{
    float pdf;
    vec3 bsdf = SampleBSDF(vertexData, pbrParams, pdf);

    if (IsBlack(bsdf) || pdf < EPSILON)
    {
        return vec3(0.0);
    }
    
    vec3 pathThroughput = rayPayload.throughput * (bsdf * pbrParams.NdotL) / pdf;
    
    if(rayPayload.bouncesCount > minDepth) {
        float terminationThreshold = max(MIN_TERMINATION_THRESHOLD, 1.0 - Maxcomp(pathThroughput));
        if(RandomFloat(rayPayload.randomSeed) < terminationThreshold) {
            return vec3(0.0);
        }
        pathThroughput /= 1.0 - terminationThreshold;
    }
    
    indirect.throughput = pathThroughput;
    indirect.randomSeed = rayPayload.randomSeed;
    indirect.bouncesCount = rayPayload.bouncesCount + 1;
    indirect.sampleEnviroment = 0;
    
    traceNV(topLevelAS, 
        gl_RayFlagsOpaqueNV,
        0xFF,
        0, 0, 0,
        pbrParams.worldPos,
        RAY_MIN,
        -pbrParams.L,
        RAY_MAX,
        1);
    
    return indirect.color;
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
        // TODO: move to uniforms
        const float enviromentFactor = 1.0f;
    
        PBRParams pbrParams = GetPBRParams(vertexData, material);
        
        rayPayload.color  = (rayPayload.bouncesCount == 0) ? pbrParams.emissive.rgb : vec3(0.0);
        rayPayload.color += GetDirectLighting(pbrParams, vertexData);
        rayPayload.color += GetEnviromentLighting(pbrParams, vertexData) * enviromentFactor;
        
        // Produce bounce
        if (rayPayload.bouncesCount < camera.bouncesCount)
        {
            rayPayload.color += GetIndirectLighting(pbrParams, vertexData, 1);   
        }        
        
        // TODO: remove this environmental reflections hack
        // Produce bounce
        if (rayPayload.bouncesCount < camera.bouncesCount)
        {
            rayPayload.color += GetReflections(pbrParams);   
        }
    }
    else
    {
        rayPayload.color = vec3(1.0, 0.0, 0.0);
    }
}
