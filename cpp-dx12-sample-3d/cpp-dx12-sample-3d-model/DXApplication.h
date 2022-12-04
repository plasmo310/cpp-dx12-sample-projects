#pragma once
#include <d3d12.h>
#include <dxgi1_6.h>
#include <D3Dcompiler.h>
#include <DirectXMath.h>
#include <DirectXTex.h>

#include <wrl.h>
#include "d3dx12.h"

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
	ComPtr<ID3D12DescriptorHeap> basicHeap_;            // 基本情報の受け渡し用(SRV + CBV)
	ComPtr<ID3D12Resource> renderTargets_[kFrameCount]; // バックバッファー
	ComPtr<ID3D12PipelineState> pipelinestate_;         // パイプラインステート
	ComPtr<ID3D12RootSignature> rootsignature_;         // ルートシグネチャ

	// リソース
	ComPtr<ID3D12Resource> vertexBuffer_;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView_;
	ComPtr<ID3D12Resource> indexBuffer_;
	D3D12_INDEX_BUFFER_VIEW indexBufferView_;
	ComPtr<ID3D12Resource> constBuffer_;
	ComPtr<ID3D12Resource> textureBuffer_;

	struct Vertex
	{
		DirectX::XMFLOAT3 pos;
		DirectX::XMFLOAT2 uv;
	};

	// フェンス
	ComPtr<ID3D12Fence> fence_;
	UINT64 fenceValue_;
	HANDLE fenceEvent_;

	// 3D座標変換用行列
	DirectX::XMMATRIX worldMatrix_;
	DirectX::XMMATRIX viewMatrix_;
	DirectX::XMMATRIX projMatrix_;
	DirectX::XMMATRIX* mapMatrix_;

	// オブジェクトのパラメータを想定
	float angle_ = 30.0f;
	float scale_ = 3.0f;

	void LoadPipeline(HWND hwnd);
	void LoadAssets();

	void CreateD3D12Device(IDXGIFactory6* dxgiFactory, ID3D12Device** d3d12device);
	void ThrowIfFailed(HRESULT hr);
};
