#include"BasicType.hlsli"

Texture2D g_texture : register(t0);
SamplerState g_sampler : register(s0);

float4 BasicPS(BasicType input) : SV_TARGET
{
	return g_texture.Sample(g_sampler, input.uv);
}