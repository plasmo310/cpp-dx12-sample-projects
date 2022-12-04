#pragma once
#include <d3d12.h>
#include <dxgi1_6.h>
#include <D3Dcompiler.h>
#include <DirectXMath.h>
#include <DirectXTex.h>

#include <wrl.h>
#include "d3dx12.h"

#include "FbxLoader.h"

#include <stdexcept>
#include <vector>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "DirectXTex.lib")

using Microsoft::WRL::ComPtr;

class DXApplication
{
public:
	DXApplication(unsigned int width, unsigned int height, std::wstring title);
	void OnInit(HWND hwnd);
	void OnUpdate();
	void OnRender();
	void OnDestroy();

	const WCHAR* GetTitle() const { return title_.c_str(); }
	unsigned int GetWindowWidth() const { return windowWidth_; }
	unsigned int GetWindowHeight() const { return windowHeight_; }
private:
	static const unsigned int kFrameCount = 2;

	std::wstring title_;
	unsigned int windowWidth_;
	unsigned int windowHeight_;

	CD3DX12_VIEWPORT viewport_; // ビューポート
	CD3DX12_RECT scissorrect_;  // シザー短形

	// パイプラインオブジェクト
	ComPtr<ID3D12Device> device_;
	ComPtr<ID3D12CommandAllocator> commandAllocator_;
	ComPtr<ID3D12GraphicsCommandList> commandList_;
	ComPtr<ID3D12CommandQueue> commandQueue_;
	ComPtr<IDXGISwapChain4> swapchain_;
	ComPtr<ID3D12DescriptorHeap> rtvHeap_;              // レンダーターゲットヒープ
	ComPtr<ID3D12DescriptorHeap> dsvHeap_;              // 深度バッファーヒープ
	ComPtr<ID3D12DescriptorHeap> basicHeap_;            // 基本情報の受け渡し用(SRV + CBV)
	ComPtr<ID3D12Resource> renderTargets_[kFrameCount]; // バックバッファー
	ComPtr<ID3D12Resource> depthBuffer_;                // 深度バッファー
	ComPtr<ID3D12PipelineState> pipelinestate_;         // パイプラインステート
	ComPtr<ID3D12RootSignature> rootsignature_;         // ルートシグネチャ

	// リソース
	ComPtr<ID3D12Resource> vertexBuffer_;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView_;
	ComPtr<ID3D12Resource> indexBuffer_;
	D3D12_INDEX_BUFFER_VIEW indexBufferView_;
	ComPtr<ID3D12Resource> constMatricesBuffer_;
	ComPtr<ID3D12Resource> constLightingBuffer_;
	ComPtr<ID3D12Resource> textureBuffer_;

	// フェンス
	ComPtr<ID3D12Fence> fence_;
	UINT64 fenceValue_;
	HANDLE fenceEvent_;

	// 3D座標変換用行列
	DirectX::XMMATRIX worldMatrix_;
	DirectX::XMMATRIX viewMatrix_;
	DirectX::XMMATRIX projMatrix_;
	struct MatricesData
	{
		DirectX::XMMATRIX world;
		DirectX::XMMATRIX viewproj;
	};
	MatricesData* mapMatricesData_;

	// ライティング用
	DirectX::XMVECTOR ambientLight_ = { 0.35f, 0.35f, 0.35f };
	DirectX::XMVECTOR lightColor_ = { 0.8f, 0.8f, 1.0f };
	DirectX::XMVECTOR lightDirection_ = { 0.3f, 0.3f, 0.8f };
	struct LightingData
	{
		DirectX::XMVECTOR ambientLight;
		DirectX::XMVECTOR lightColor;
		DirectX::XMVECTOR lightDirection;
	};
	LightingData* mapLightingData_;

	// オブジェクトのパラメータを想定
	// ゴロヤン
	float angle_ = 30.0f;
	float scale_ = 3.5f;
	DirectX::XMFLOAT3 translate_ = { 0.0f, -2.5f, -0.25f };
	// サイコロ
	//float angle_ = 30.0f;
	//float scale_ = 1.0f;
	//DirectX::XMFLOAT3 translate_ = { 0.0f, 0.0f, -0.0f };

	FbxLoader::VertexInfo fbxVertexInfo_;

	void LoadPipeline(HWND hwnd);
	void LoadAssets();

	void CreateD3D12Device(IDXGIFactory6* dxgiFactory, ID3D12Device** d3d12device);
	void ThrowIfFailed(HRESULT hr);
	void ThrowMessage(std::string message);
};
