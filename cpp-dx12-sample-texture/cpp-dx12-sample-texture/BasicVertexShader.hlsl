#include"BasicType.hlsli"
BasicType BasicVS(float4 pos : POSITION, float2 uv : TEXCOORD)
{
	BasicType output;
	output.position = pos;
	output.uv = uv;
	return output;
}