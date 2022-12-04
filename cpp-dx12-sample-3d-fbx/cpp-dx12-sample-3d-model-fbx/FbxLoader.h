#pragma once
#include <fbxsdk.h>
#include <vector>
#include <array>
#include <string>

#pragma comment(lib, "libfbxsdk-md.lib")
#pragma comment(lib, "libxml2-md.lib")
#pragma comment(lib, "zlib-md.lib")

class FbxLoader
{
public:
	FbxLoader();

	struct Vertex
	{
		float pos[3];
		float normal[3];
		float uv[2];
	};
	struct VertexInfo
	{
		std::vector<Vertex> vertices;
		std::vector<unsigned short> indices;
	};

	static bool Load(const std::string& filePath, VertexInfo* vertexInfo);

private:
	static bool IsExistNormalUVInfo(const std::vector<float>& vertexInfo);
	static std::vector<float> CreateVertexInfo(const std::vector<float>& vertex, const FbxVector4& normalVec4, const FbxVector2& uvVec2);
	static int CreateNewVertexIndex(const std::vector<float>& vertexInfo, const FbxVector4& normalVec4, const FbxVector2& uvVec2,
		std::vector<std::vector<float>>& vertexInfoList, int oldIndex, std::vector<std::array<int, 2>>& oldNewIndexPairList);
	static bool IsSetNormalUV(const std::vector<float> vertexInfo, const FbxVector4& normalVec4, const FbxVector2& uvVec2);
};
