#version 450

layout (set = 1, binding = 0) uniform sampler2D samplerAlbedoMap;
layout (set = 1, binding = 1) uniform sampler2D samplerNormalMap;
layout (set = 1, binding = 2) uniform sampler2D samplerOcclusionRoughnessMetallicMap;

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec3 inColor;
layout (location = 2) in vec2 inUV;
layout (location = 3) in vec3 inViewVec;
layout (location = 4) in vec3 inLightVec;
layout (location = 5) in vec3 inWorldPos;
layout (location = 6) in vec3 inLightPos;

layout (location = 0) out vec4 outFragColor;

const vec3 LIGHT_COLOR = vec3(1.0);
const float DIELECTRIC_REFLECTION_APPROXIMATION = 0.04;
const float PI = 3.14159265359;

// Approximation of microfacets towards half-vector using Normal Distribution
float NDF_GGXTR(vec3 N, vec3 H, float roughness)
{
    float a2 = roughness * roughness;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;
	
    float nom = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
	
    return nom / denom;
}

// Geometry function
float G_SchlickGGX(float NdotV, float k)
{
    float nom = NdotV;
    float denom = NdotV * (1.0 - k) + k;
	
    return nom / denom;
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

// Simple squared attenuation
float calculateAttenuation(vec3 fragPos, vec3 lightPos)
{
    float distance = length(fragPos - lightPos);
    return 1.0;
}

// Result reflection
vec3 BRDF_CookTorrance(float occlusion, float roughness, float metalness, vec3 albedo, vec3 N, vec3 V, vec3 L)
{
    // For each light source...
    float lightAttenuation = calculateAttenuation(inWorldPos, inLightPos);
    vec3 radiance = LIGHT_COLOR * lightAttenuation;
    
    // We want to find material indices of refraction for Fresnel approximation
    vec3 F0 = vec3(DIELECTRIC_REFLECTION_APPROXIMATION);
    F0 = mix(F0, albedo.rgb, metalness);
    
    vec3 H = normalize(V + L);
    
    float D = NDF_GGXTR(N, H, roughness);
    float G = G_Smith(N, V, L, roughness);
    vec3 F = F_Schlick(max(dot(H, V), 0.0), F0);
    
    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metalness;	  
    
    vec3 DFG = D * F * G;
    
    // Zero division pre-fire
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.001;
    vec3 specular = DFG / denominator; 
    
    vec3 lambert = kD * albedo / PI;
    
    float NdotL = max(dot(N, L), 0.0);        
    return (lambert + specular) * radiance * NdotL;
}

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
    vec3 L = -normalize(inLightVec);
    vec3 N = perturbNormal();
	vec3 V = normalize(inViewVec);
    vec3 R = reflect(L, N);
 
	vec4 albedo = texture(samplerAlbedoMap, inUV) * vec4(inColor, 1.0); 
    float occlusion = texture(samplerOcclusionRoughnessMetallicMap, inUV).r;
    float roughness = texture(samplerOcclusionRoughnessMetallicMap, inUV).g;
    float metalness = texture(samplerOcclusionRoughnessMetallicMap, inUV).b;
    
    vec3 brdf = BRDF_CookTorrance(occlusion, roughness, metalness, albedo.xyz, N, V, L);
    
    vec3 ambient = vec3(0.03) * albedo.xyz * occlusion;
    vec3 color = ambient + brdf;   
    
    // Tone compression 
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0/2.2));  
    
    outFragColor = vec4(color, 1.0);
}