#version 460

#ifndef SHADER_STAGE
#define SHADER_STAGE raygen
#pragma shader_stage(raygen)
#extension GL_NV_ray_tracing : require
void main() {}
#endif

struct RayPayload
{
	vec4 ColorAndDistance; // rgb + t
	vec4 ScatterDirection; // xyz + w (is scatter needed)
	uint RandomSeed;
};