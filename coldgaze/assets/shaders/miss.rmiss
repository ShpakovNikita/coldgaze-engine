#version 460
#extension GL_NV_ray_tracing : require

layout (set = 2, binding = 0) uniform sampler2D equirectangularMap;

struct RayPayload
{
	vec4 radianceAndDistance; // rgb + a (t)
	vec4 scatterDirection; // xyz + w (is scatter needed)
    vec4 ambient;
	uint randomSeed;
};

layout(location = 0) rayPayloadInNV RayPayload rayPayload;

const vec2 invAtan = vec2(0.1591, 0.3183);
vec2 SampleSphericalMap(vec3 v)
{
    vec2 uv = vec2(atan(v.z, v.x), asin(v.y));
    uv *= invAtan;
    uv += 0.5;
    return uv;
}

void main()
{
    vec2 uv = SampleSphericalMap(normalize(gl_WorldRayDirectionNV)); 
    vec3 color = texture(equirectangularMap, uv).rgb;
    
    rayPayload.ambient.rgb = texture(equirectangularMap, uv).rgb;
    rayPayload.radianceAndDistance.rgb = rayPayload.ambient.rgb;
    rayPayload.radianceAndDistance.a = 0.0;
    rayPayload.scatterDirection.w = 0.0;
}