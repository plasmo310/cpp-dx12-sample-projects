//頂点シェーダ→ピクセルシェーダへのやり取りに使用する
//構造体
struct BasicType {
	float4 position : SV_POSITION; // 頂点座標
	float2 uv : TEXCOORD;          // UV値
};