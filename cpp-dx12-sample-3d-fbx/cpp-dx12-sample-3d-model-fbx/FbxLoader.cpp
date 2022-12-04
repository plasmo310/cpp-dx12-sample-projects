#include "FbxLoader.h"

FbxLoader::FbxLoader()
{
}

bool FbxLoader::Load(const std::string& filePath, VertexInfo* vertexInfo)
{
	// マネージャー初期化
	auto manager = FbxManager::Create();

	// インポーター初期化
	auto importer = FbxImporter::Create(manager, "");
	if (!importer->Initialize(filePath.c_str(), -1, manager->GetIOSettings()))
	{
		return false;
	}

	// シーン作成
	auto scene = FbxScene::Create(manager, "");
	importer->Import(scene);
	importer->Destroy();

	// 三角ポリゴンへのコンバート
	FbxGeometryConverter geometryConverter(manager);
	if (!geometryConverter.Triangulate(scene, true))
	{
		return false;
	}

	// メッシュ取得
	auto mesh = scene->GetSrcObject<FbxMesh>();
	if (!mesh)
	{
		return false;
	}

	// UVセット名の取得
	// * 現在の実装だとは1つのUVセット名にしか対応していない...
	FbxStringList uvSetNameList;
	mesh->GetUVSetNames(uvSetNameList);
	const char* uvSetName = uvSetNameList.GetStringAt(0);

	// 頂点座標情報のリストを生成
	std::vector<std::vector<float>> vertexInfoList;
	for (int i = 0; i < mesh->GetControlPointsCount(); i++)
	{
		// 頂点座標を読み込んで設定
		auto point = mesh->GetControlPointAt(i);
		std::vector<float> vertex;
		vertex.push_back(point[0]);
		vertex.push_back(point[1]);
		vertex.push_back(point[2]);
		vertexInfoList.push_back(vertex);
	}

	// 頂点毎の情報を取得する
	std::vector<unsigned short> indices;
	std::vector<std::array<int, 2>> oldNewIndexPairList;
	for (int polIndex = 0; polIndex < mesh->GetPolygonCount(); polIndex++) // ポリゴン毎のループ
	{
		for (int polVertexIndex = 0; polVertexIndex < mesh->GetPolygonSize(polIndex); polVertexIndex++) // 頂点毎のループ
		{
			// インデックス座標
			auto vertexIndex = mesh->GetPolygonVertex(polIndex, polVertexIndex);

			// 頂点座標
			std::vector<float> vertexInfo = vertexInfoList[vertexIndex];

			// 法線座標
			FbxVector4 normalVec4;
			mesh->GetPolygonVertexNormal(polIndex, polVertexIndex, normalVec4);

			// UV座標
			FbxVector2 uvVec2;
			bool isUnMapped;
			mesh->GetPolygonVertexUV(polIndex, polVertexIndex, uvSetName, uvVec2, isUnMapped);

			// インデックス座標のチェックと再採番
			if (!IsExistNormalUVInfo(vertexInfo))
			{
				// 法線座標とUV座標が未設定の場合、頂点情報に付与して再設定
				vertexInfoList[vertexIndex] = CreateVertexInfo(vertexInfo, normalVec4, uvVec2);
			}
			else if (!IsSetNormalUV(vertexInfo, normalVec4, uvVec2))
			{
				// ＊同一頂点インデックスの中で法線座標かUV座標が異なる場合、
				// 新たな頂点インデックスとして作成する
				vertexIndex = CreateNewVertexIndex(vertexInfo, normalVec4, uvVec2, vertexInfoList, vertexIndex, oldNewIndexPairList);
			}

			// インデックス座標を設定
			indices.push_back(vertexIndex);
		}
	}


	// 頂点情報を生成
	std::vector<Vertex> vertices;
	for (int i = 0; i < vertexInfoList.size(); i++)
	{
		std::vector<float> vertexInfo = vertexInfoList[i];
		vertices.push_back(Vertex{
			{
				vertexInfo[0], vertexInfo[1], vertexInfo[2]
			},
			{
				vertexInfo[3], vertexInfo[4], vertexInfo[5]
			},
			{
				vertexInfo[6], 1.0f - vertexInfo[7] // Blenderで作成した場合、V値は反転させる
			}
		});
	}
	

	// マネージャー、シーンの破棄
	scene->Destroy();
	manager->Destroy();

	// 返却値に設定
	*vertexInfo = {
		vertices,
		indices
	};

	return true;
}

// 法線、UV情報が存在しているか？
bool FbxLoader::IsExistNormalUVInfo(const std::vector<float>& vertexInfo)
{
	return vertexInfo.size() == 8; // 頂点3 + 法線3 + UV2
}

// 頂点情報を生成
std::vector<float> FbxLoader::CreateVertexInfo(const std::vector<float>& vertexInfo, const FbxVector4& normalVec4, const FbxVector2& uvVec2)
{
	std::vector<float> newVertexInfo;
	// 位置座標
	newVertexInfo.push_back(vertexInfo[0]);
	newVertexInfo.push_back(vertexInfo[1]);
	newVertexInfo.push_back(vertexInfo[2]);
	// 法線座標
	newVertexInfo.push_back(normalVec4[0]);
	newVertexInfo.push_back(normalVec4[1]);
	newVertexInfo.push_back(normalVec4[2]);
	// UV座標
	newVertexInfo.push_back(uvVec2[0]);
	newVertexInfo.push_back(uvVec2[1]);
	return newVertexInfo;
}

// 新たな頂点インデックスを生成する
int FbxLoader::CreateNewVertexIndex(const std::vector<float>& vertexInfo, const FbxVector4& normalVec4, const FbxVector2& uvVec2,
	std::vector<std::vector<float>>& vertexInfoList, int oldIndex, std::vector<std::array<int, 2>>& oldNewIndexPairList)
{
	// 作成済の場合、該当のインデックスを返す
	for (int i = 0; i < oldNewIndexPairList.size(); i++)
	{
		int newIndex = oldNewIndexPairList[i][1];
		if (oldIndex == oldNewIndexPairList[i][0]
			&& IsSetNormalUV(vertexInfoList[newIndex], normalVec4, uvVec2))
		{
			return newIndex;
		}
	}
	// 作成済でない場合、新たな頂点インデックスとして作成
	std::vector<float> newVertexInfo = CreateVertexInfo(vertexInfo, normalVec4, uvVec2);
	vertexInfoList.push_back(newVertexInfo);
	// 作成したインデックス情報を設定
	int newIndex = vertexInfoList.size() - 1;
	std::array<int, 2> oldNewIndexPair{ oldIndex , newIndex };
	oldNewIndexPairList.push_back(oldNewIndexPair);
	return newIndex;
}

// vertexInfoに法線、UV座標が設定済かどうか？
bool FbxLoader::IsSetNormalUV(const std::vector<float> vertexInfo, const FbxVector4& normalVec4, const FbxVector2& uvVec2)
{
	// 法線、UV座標が同値なら設定済とみなす
	return fabs(vertexInfo[3] - normalVec4[0]) < FLT_EPSILON
		&& fabs(vertexInfo[4] - normalVec4[1]) < FLT_EPSILON
		&& fabs(vertexInfo[5] - normalVec4[2]) < FLT_EPSILON
		&& fabs(vertexInfo[6] - uvVec2[0]) < FLT_EPSILON
		&& fabs(vertexInfo[7] - uvVec2[1]) < FLT_EPSILON;
}
