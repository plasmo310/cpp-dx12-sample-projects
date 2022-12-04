#include"BasicType.hlsli"

cbuffer cbuff0 : register(b0) {
	matrix mat; // •ÏŠ·s—ñ
}

BasicType BasicVS(float4 pos : POSITION, float2 uv : TEXCOORD)
{
	BasicType output;
	output.position = mul(mat, pos); // HLSL‚Å‚Í—ñ—Dæ
	output.uv = uv;
	return output;
}