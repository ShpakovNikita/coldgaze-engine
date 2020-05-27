#version 450

layout (set = 1, binding = 0) uniform sampler2D equirectangularMap;

layout (location = 0) in vec3 inWorldPos;

layout (location = 0) out vec4 outFragColor;

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
    vec2 uv = SampleSphericalMap(normalize(inWorldPos)); 
    vec3 color = texture(equirectangularMap, uv).rgb;
    
    outFragColor = vec4(color, 1.0);
}