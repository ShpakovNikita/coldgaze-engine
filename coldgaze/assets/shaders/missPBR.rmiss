#version 460
#extension GL_NV_ray_tracing : require

layout (set = 2, binding = 0) uniform sampler2D equirectangularMap;

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
    
    if (rayPayload.sampleEnviroment > 0.0)
    {
        rayPayload.color = texture(equirectangularMap, uv).rgb;
    }
    else
    {
        rayPayload.color = vec3(0.0);
    }
    
    rayPayload.envHit = 1;
}