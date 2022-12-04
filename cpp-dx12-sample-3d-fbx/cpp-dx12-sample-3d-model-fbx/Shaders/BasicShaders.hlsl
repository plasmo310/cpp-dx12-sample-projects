
cbuffer cbuff0 : register(b0) {
	matrix world;
	matrix viewproj;
}

cbuffer cbuff1 : register(b1) {
	float3 ambientLight;
	float3 lightColor;
	float3 lightDirection;
}

Texture2D g_texture : register(t0);
SamplerState g_sampler : register(s0);

struct PSInput
{
	float4 position : SV_POSITION;
	float3 normal : NORMAL;
	float2 uv : TEXCOORD;
};

PSInput VSMain(float4 pos : POSITION, float3 normal : NORMAL, float2 uv : TEXCOORD)
{
	PSInput result;
	result.position = mul(mul(viewproj, world), pos);
	result.normal = mul(world, normal);
	result.uv = uv;
	return result;
}

float4 PSMain(PSInput input) : SV_TARGET
{
	return g_texture.Sample(g_sampler, input.uv);
}
