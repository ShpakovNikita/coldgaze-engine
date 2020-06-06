#version 460
#extension GL_NV_ray_tracing : require

struct RayPayload
{
	vec4 colorAndDistance; // rgb + t
	vec4 scatterDirection; // xyz + w (is scatter needed)
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
    rayPayload.colorAndDistance.xyz = vec3(0.0, 0.0, 0.2);
}