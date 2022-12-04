#include "DXApplication.h"
#include "Win32Application.h"

DXApplication::DXApplication(unsigned int width, unsigned int height, std::wstring title)
	: title_(title)
	, windowWidth_(width)
	, windowHeight_(height)
	, viewport_(0.0f, 0.0f, static_cast<float>(windowWidth_), static_cast<float>(windowHeight_))
	, scissorrect_(0, 0, static_cast<LONG>(windowWidth_), static_cast<LONG>(windowHeight_))
	, vertexBufferView_({})
	, indexBufferView_({})
	, fenceValue_(0)
	, fenceEvent_(nullptr)
{
}

// 初期化処理
void DXApplication::OnInit(HWND hwnd)
{
	LoadPipeline(hwnd);
	LoadAssets();
}

void DXApplication::LoadPipeline(HWND hwnd)
{
	UINT dxgiFactoryFlags = 0;

#ifdef _DEBUG
	{
		// デバッグレイヤーを有効にする
		ComPtr<ID3D12Debug> debugLayer;
		if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugLayer)))) {
			debugLayer->EnableDebugLayer();
			dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
		}
	}
#endif

	// DXGIFactoryの初期化
	ComPtr<IDXGIFactory6> dxgiFactory;
	ThrowIfFailed(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&dxgiFactory)));

	// デバイスの初期化
	CreateD3D12Device(dxgiFactory.Get(), device_.ReleaseAndGetAddressOf());

	// コマンド関連の初期化
	{
		// コマンドキュー
		D3D12_COMMAND_QUEUE_DESC commandQueueDesc = {};
		commandQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE; // タイムアウト無し
		commandQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT; // コマンドリストと合わせる
		ThrowIfFailed(device_->CreateCommandQueue(&commandQueueDesc, IID_PPV_ARGS(commandQueue_.ReleaseAndGetAddressOf())));
		// コマンドアロケータ
		ThrowIfFailed(device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(commandAllocator_.ReleaseAndGetAddressOf())));
		// コマンドリスト
		ThrowIfFailed(device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator_.Get(), nullptr, IID_PPV_ARGS(commandList_.ReleaseAndGetAddressOf())));
	}

	// スワップチェーンの初期化
	{
		DXGI_SWAP_CHAIN_DESC1 swapchainDesc = {};
		swapchainDesc.BufferCount = kFrameCount;
		swapchainDesc.Width = windowWidth_;
		swapchainDesc.Height = windowHeight_;
		swapchainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		swapchainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapchainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		swapchainDesc.SampleDesc.Count = 1;
		ThrowIfFailed(dxgiFactory->CreateSwapChainForHwnd(
			commandQueue_.Get(),
			hwnd,
			&swapchainDesc,
			nullptr,
			nullptr,
			(IDXGISwapChain1**)swapchain_.ReleaseAndGetAddressOf()));
	}

	// ディスクリプタヒープの初期化
	{
		// レンダーターゲットビュー
		D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
		rtvHeapDesc.NumDescriptors = kFrameCount;
		rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		ThrowIfFailed(device_->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(rtvHeap_.ReleaseAndGetAddressOf())));
		// 深度バッファービュー
		D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
		dsvHeapDesc.NumDescriptors = 1;
		dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
		ID3D12DescriptorHeap* dsvHeap = nullptr;
		ThrowIfFailed(device_->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(dsvHeap_.ReleaseAndGetAddressOf())));
		// 基本情報の受け渡し用
		D3D12_DESCRIPTOR_HEAP_DESC basicHeapDesc = {};
		basicHeapDesc.NumDescriptors = 3; // 1SRV + 2CBV
		basicHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		basicHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		ThrowIfFailed(device_->CreateDescriptorHeap(&basicHeapDesc, IID_PPV_ARGS(basicHeap_.ReleaseAndGetAddressOf())));
	}

	// スワップチェーンと関連付けてレンダーターゲットビューを生成
	{
		CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(rtvHeap_->GetCPUDescriptorHandleForHeapStart());
		for (UINT i = 0; i < kFrameCount; ++i)
		{
			ThrowIfFailed(swapchain_->GetBuffer(static_cast<UINT>(i), IID_PPV_ARGS(renderTargets_[i].ReleaseAndGetAddressOf())));
			device_->CreateRenderTargetView(renderTargets_[i].Get(), nullptr, rtvHandle);
			rtvHandle.Offset(1, device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV));
		}
	}

	// 深度バッファービュー生成
	{
		// 深度バッファー作成
		D3D12_RESOURCE_DESC depthResDesc = {};
		depthResDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		depthResDesc.Width = windowWidth_;
		depthResDesc.Height = windowHeight_;
		depthResDesc.DepthOrArraySize = 1;
		depthResDesc.Format = DXGI_FORMAT_D32_FLOAT;
		depthResDesc.SampleDesc.Count = 1;
		depthResDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
		depthResDesc.MipLevels = 1;
		depthResDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		depthResDesc.Alignment = 0;
		auto depthHeapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
		// クリアバリューの設定
		D3D12_CLEAR_VALUE _depthClearValue = {};
		_depthClearValue.DepthStencil.Depth = 1.0f;      //深さ１(最大値)でクリア
		_depthClearValue.Format = DXGI_FORMAT_D32_FLOAT; //32bit深度値としてクリア
		ThrowIfFailed(device_->CreateCommittedResource(
			&depthHeapProp,
			D3D12_HEAP_FLAG_NONE,
			&depthResDesc,
			D3D12_RESOURCE_STATE_DEPTH_WRITE, //デプス書き込みに使用
			&_depthClearValue,
			IID_PPV_ARGS(depthBuffer_.ReleaseAndGetAddressOf())));
		// 深度バッファービュー作成
		D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
		dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
		dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
		device_->CreateDepthStencilView(depthBuffer_.Get(), &dsvDesc, dsvHeap_->GetCPUDescriptorHandleForHeapStart());
	}

	// フェンスの生成
	{
		ThrowIfFailed(device_->CreateFence(fenceValue_, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(fence_.ReleaseAndGetAddressOf())));
		fenceEvent_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	}
}

void DXApplication::LoadAssets()
{
	// ルートシグネチャの生成
	{
		// ルートパラメータの生成
		// ディスクリプタテーブルの実体
		CD3DX12_DESCRIPTOR_RANGE1 discriptorRanges[2];
		discriptorRanges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 2, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC); // CBV
		discriptorRanges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC); // SRV
		CD3DX12_ROOT_PARAMETER1 rootParameters[1];
		rootParameters[0].InitAsDescriptorTable(2, discriptorRanges, D3D12_SHADER_VISIBILITY_ALL); // 同一パラメータで複数指定
		// サンプラーの生成
		// テクスチャデータからどう色を取り出すかを決めるための設定
		D3D12_STATIC_SAMPLER_DESC sampler = {};
		sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
		sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		sampler.MipLODBias = 0;
		sampler.MaxAnisotropy = 0;
		sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
		sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
		sampler.MinLOD = 0.0f;
		sampler.MaxLOD = D3D12_FLOAT32_MAX;
		sampler.ShaderRegister = 0;
		sampler.RegisterSpace = 0;
		sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		// ルートパラメータ、サンプラーからルートシグネチャを生成
		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
		rootSignatureDesc.Init_1_1(_countof(rootParameters), rootParameters, 1, &sampler, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
		ComPtr<ID3DBlob> rootSignatureBlob = nullptr;
		ComPtr<ID3DBlob> errorBlob = nullptr;
		ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1_0, &rootSignatureBlob, &errorBlob));
		ThrowIfFailed(device_->CreateRootSignature(0, rootSignatureBlob->GetBufferPointer(), rootSignatureBlob->GetBufferSize(), IID_PPV_ARGS(rootsignature_.ReleaseAndGetAddressOf())));
	}

	// パイプラインステートの生成
	{
		// シェーダーオブジェクトの生成
#if defined(_DEBUG)
		UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
		UINT compileFlags = 0;
#endif
		ComPtr<ID3DBlob> vsBlob;
		ComPtr<ID3DBlob> psBlob;
		D3DCompileFromFile(L"Shaders/LambertShaders.hlsl", nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "VSMain", "vs_5_0", compileFlags, 0, &vsBlob, nullptr);
		D3DCompileFromFile(L"Shaders/LambertShaders.hlsl", nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "PSMain", "ps_5_0", compileFlags, 0, &psBlob, nullptr);

		// 頂点レイアウトの生成
		D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
		};

		// 裏面描画にする場合、コメントを外すべし
		auto rasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		//rasterizerState.CullMode = D3D12_CULL_MODE_NONE;

		// パイプラインステートオブジェクト(PSO)を生成
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.pRootSignature = rootsignature_.Get();
		psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) }; // 入力レイアウトの設定
		psoDesc.VS = CD3DX12_SHADER_BYTECODE(vsBlob.Get());                       // 頂点シェーダ
		psoDesc.PS = CD3DX12_SHADER_BYTECODE(psBlob.Get());                       // ピクセルシェーダ
		psoDesc.RasterizerState = rasterizerState;                                // ラスタライザーステート
		psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);                   // ブレンドステート
		psoDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;                           // サンプルマスクの設定
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;   // トポロジタイプ
		psoDesc.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;    // ストリップ時のカット設定
		psoDesc.NumRenderTargets = 1;                                             // レンダーターゲット数
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;                       // レンダーターゲットフォーマット
		psoDesc.SampleDesc.Count = 1;                                             // マルチサンプリングの設定
		// 深度ステンシル 
		psoDesc.DepthStencilState.DepthEnable = true;                             // 深度バッファーを使用するか
		psoDesc.DepthStencilState.StencilEnable = false;                          // ステンシルテストを行うか
		psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;                                // 深度バッファーで使用するフォーマット
		psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;         // 書き込む
		psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;    // 小さい方を採用する
		ThrowIfFailed(device_->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(pipelinestate_.ReleaseAndGetAddressOf())));
	}

	// fbxモデルのロード
	{
		if (!FbxLoader::Load("Assets/goloyan.fbx", &fbxVertexInfo_))
		{
			ThrowMessage("failed load fbx file.");
		}
	}

	// 頂点バッファビューの生成
	{
		// 頂点座標
		std::vector<FbxLoader::Vertex> vertices = fbxVertexInfo_.vertices;
		const UINT vertexBufferSize = sizeof(FbxLoader::Vertex) * vertices.size();
		auto vertexHeapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
		auto vertexResDesc = CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize);
		// 頂点バッファーの生成
		ThrowIfFailed(device_->CreateCommittedResource(
			&vertexHeapProp,
			D3D12_HEAP_FLAG_NONE,
			&vertexResDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(vertexBuffer_.ReleaseAndGetAddressOf())));
		// 頂点情報のコピー
		FbxLoader::Vertex* vertexMap = nullptr;
		ThrowIfFailed(vertexBuffer_->Map(0, nullptr, (void**)&vertexMap));
		std::copy(std::begin(vertices), std::end(vertices), vertexMap);
		vertexBuffer_->Unmap(0, nullptr);
		// 頂点バッファービューの生成
		vertexBufferView_.BufferLocation = vertexBuffer_->GetGPUVirtualAddress();
		vertexBufferView_.SizeInBytes = vertexBufferSize;
		vertexBufferView_.StrideInBytes = sizeof(FbxLoader::Vertex);
	}

	// インデックスバッファビューの生成
	{
		// インデックス座標
		std::vector<unsigned short> indices = fbxVertexInfo_.indices;
		const UINT indexBufferSize = sizeof(unsigned short) * indices.size();
		auto indexHeapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
		auto indexResDesc = CD3DX12_RESOURCE_DESC::Buffer(indexBufferSize);
		// インデックスバッファの生成
		ThrowIfFailed(device_->CreateCommittedResource(
			&indexHeapProp,
			D3D12_HEAP_FLAG_NONE,
			&indexResDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(indexBuffer_.ReleaseAndGetAddressOf())));
		// インデックス情報のコピー
		unsigned short* indexMap = nullptr;
		indexBuffer_->Map(0, nullptr, (void**)&indexMap);
		std::copy(std::begin(indices), std::end(indices), indexMap);
		indexBuffer_->Unmap(0, nullptr);
		// インデックスバッファビューの生成
		indexBufferView_.BufferLocation = indexBuffer_->GetGPUVirtualAddress();
		indexBufferView_.SizeInBytes = indexBufferSize;
		indexBufferView_.Format = DXGI_FORMAT_R16_UINT;
	}

	// テクスチャのロード処理
	DirectX::TexMetadata metadata = {};
	DirectX::ScratchImage scratchImg = {};
	std::vector<D3D12_SUBRESOURCE_DATA> textureSubresources;
	{
		ThrowIfFailed(LoadFromWICFile(L"Assets/tex_goloyan.png", DirectX::WIC_FLAGS_NONE, &metadata, scratchImg));
		ThrowIfFailed(PrepareUpload(device_.Get(), scratchImg.GetImages(), scratchImg.GetImageCount(), metadata, textureSubresources));
	}

	// ディスクリプタヒープもハンドルを事前に取得
	auto basicHeapHandle = basicHeap_->GetCPUDescriptorHandleForHeapStart();

	// 座標変換マトリクス(CBV)の生成
	{
		// 定数バッファーの生成
		auto constHeapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
		auto constDesc = CD3DX12_RESOURCE_DESC::Buffer((sizeof(mapMatricesData_) + 0xff) & ~0xff); // 256アライメントでサイズを指定
		ThrowIfFailed(device_->CreateCommittedResource(
			&constHeapProp,
			D3D12_HEAP_FLAG_NONE,
			&constDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(constMatricesBuffer_.ReleaseAndGetAddressOf())));
		// 定数バッファービューの生成
		D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
		cbvDesc.BufferLocation = constMatricesBuffer_->GetGPUVirtualAddress();
		cbvDesc.SizeInBytes = static_cast<UINT>(constMatricesBuffer_->GetDesc().Width);
		device_->CreateConstantBufferView(&cbvDesc, basicHeapHandle);
		// 3D座標用の行列を生成
		worldMatrix_ = DirectX::XMMatrixScaling(scale_, scale_, scale_)
			* DirectX::XMMatrixRotationY(angle_ * (DirectX::XM_PI / 180.0f))
			* DirectX::XMMatrixTranslation(translate_.x, translate_.y, translate_.z);
		DirectX::XMFLOAT3 eye(0, 0, -5);
		DirectX::XMFLOAT3 target(0, 0, 0);
		DirectX::XMFLOAT3 up(0, 1, 0);
		viewMatrix_ = DirectX::XMMatrixLookAtLH(DirectX::XMLoadFloat3(&eye), DirectX::XMLoadFloat3(&target), DirectX::XMLoadFloat3(&up));
		projMatrix_ = DirectX::XMMatrixPerspectiveFovLH(
			DirectX::XM_PIDIV2, // 画角: 90度
			static_cast<float>(windowWidth_) / static_cast<float>(windowHeight_), // アスペクト比
			1.0f, // near
			10.0f // far
		);
		// 定数情報のコピー
		constMatricesBuffer_->Map(0, nullptr, (void**)&mapMatricesData_);
		mapMatricesData_->world = worldMatrix_;
		mapMatricesData_->viewproj = viewMatrix_ * projMatrix_;
		constMatricesBuffer_->Unmap(0, nullptr);
	}

	basicHeapHandle.ptr += device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	// ライティング用データ(CBV)の生成
	{
		// 定数バッファーの生成
		auto constHeapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
		auto constDesc = CD3DX12_RESOURCE_DESC::Buffer((sizeof(mapLightingData_) + 0xff) & ~0xff);
		ThrowIfFailed(device_->CreateCommittedResource(
			&constHeapProp,
			D3D12_HEAP_FLAG_NONE,
			&constDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(constLightingBuffer_.ReleaseAndGetAddressOf())));
		// 定数バッファービューの生成
		D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
		cbvDesc.BufferLocation = constLightingBuffer_->GetGPUVirtualAddress();
		cbvDesc.SizeInBytes = static_cast<UINT>(constLightingBuffer_->GetDesc().Width);
		device_->CreateConstantBufferView(&cbvDesc, basicHeapHandle);
		// 定数情報のコピー
		constLightingBuffer_->Map(0, nullptr, (void**)&mapLightingData_);
		mapLightingData_->ambientLight = ambientLight_;
		mapLightingData_->lightColor = lightColor_;
		mapLightingData_->lightDirection = lightDirection_;
		constLightingBuffer_->Unmap(0, nullptr);
	}

	basicHeapHandle.ptr += device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	// シェーダーリソースビュー(SRV)の生成
	{
		// テクスチャバッファの生成
		D3D12_RESOURCE_DESC textureDesc = {};
		textureDesc.Format = metadata.format;
		textureDesc.Width = static_cast<UINT>(metadata.width);
		textureDesc.Height = static_cast<UINT>(metadata.height);
		textureDesc.DepthOrArraySize = static_cast<UINT16>(metadata.arraySize);
		textureDesc.MipLevels = metadata.mipLevels;
		textureDesc.SampleDesc.Count = 1;
		textureDesc.SampleDesc.Quality = 0;
		textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
		auto textureHeapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
		ThrowIfFailed(device_->CreateCommittedResource(
			&textureHeapProp,
			D3D12_HEAP_FLAG_NONE,
			&textureDesc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(textureBuffer_.ReleaseAndGetAddressOf())));
		// シェーダーリソースビューの生成
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = textureDesc.Format;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = 1;
		device_->CreateShaderResourceView(textureBuffer_.Get(), &srvDesc, basicHeapHandle);
	}

	// テクスチャアップロード用バッファの生成
	ComPtr<ID3D12Resource> textureUploadBuffer;
	{
		const UINT64 textureBufferSize = GetRequiredIntermediateSize(textureBuffer_.Get(), 0, 1);
		auto textureUploadHeapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
		auto textureUploadDesc = CD3DX12_RESOURCE_DESC::Buffer(textureBufferSize);
		ThrowIfFailed(device_->CreateCommittedResource(
			&textureUploadHeapProp,
			D3D12_HEAP_FLAG_NONE,
			&textureUploadDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&textureUploadBuffer)));
	}

	// コマンドリストの生成
	{
		// テクスチャバッファの転送
		UpdateSubresources(commandList_.Get(), textureBuffer_.Get(), textureUploadBuffer.Get(), 0, 0, textureSubresources.size(), textureSubresources.data());
		auto uploadResourceBarrier = CD3DX12_RESOURCE_BARRIER::Transition(textureBuffer_.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		commandList_->ResourceBarrier(1, &uploadResourceBarrier);

		// 命令のクローズ
		commandList_->Close();
	}

	// コマンドリストの実行
	{
		ID3D12CommandList* commandLists[] = { commandList_.Get() };
		commandQueue_->ExecuteCommandLists(1, commandLists);
	}

	// GPU処理の終了を待機
	{
		ThrowIfFailed(commandQueue_->Signal(fence_.Get(), ++fenceValue_));
		if (fence_->GetCompletedValue() < fenceValue_) {
			ThrowIfFailed(fence_->SetEventOnCompletion(fenceValue_, fenceEvent_));
			WaitForSingleObject(fenceEvent_, INFINITE);
		}
	}
}

// 更新処理
void DXApplication::OnUpdate()
{
	// 回転させてみる
	angle_ += 1.0f;
	worldMatrix_ = worldMatrix_ = DirectX::XMMatrixScaling(scale_, scale_, scale_)
		* DirectX::XMMatrixRotationY(angle_ * (DirectX::XM_PI / 180.0f))
		* DirectX::XMMatrixTranslation(translate_.x, translate_.y, translate_.z);
	mapMatricesData_->world = worldMatrix_;
	mapMatricesData_->viewproj = viewMatrix_ * projMatrix_;
}

// 描画処理
void DXApplication::OnRender()
{
	// コマンドリストのリセット
	{
		ThrowIfFailed(commandAllocator_->Reset());
		ThrowIfFailed(commandList_->Reset(commandAllocator_.Get(), pipelinestate_.Get()));
	}

	// コマンドリストの生成
	{
		// バックバッファのインデックスを取得
		auto frameIndex = swapchain_->GetCurrentBackBufferIndex();

		// リソースバリアの設定 (PRESENT -> RENDER_TARGET)
		auto startResourceBarrier = CD3DX12_RESOURCE_BARRIER::Transition(renderTargets_[frameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
		commandList_->ResourceBarrier(1, &startResourceBarrier);

		// パイプラインステートと必要なオブジェクトを設定
		commandList_->SetPipelineState(pipelinestate_.Get());         // パイプラインステート
		commandList_->SetGraphicsRootSignature(rootsignature_.Get()); // ルートシグネチャ
		commandList_->RSSetViewports(1, &viewport_);                  // ビューポート
		commandList_->RSSetScissorRects(1, &scissorrect_);            // シザー短形
		// ディスクリプタテーブル
		// ルートパラメータとディスクリプタヒープを紐づける
		ID3D12DescriptorHeap* ppHeaps[] = { basicHeap_.Get() };
		commandList_->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
		commandList_->SetGraphicsRootDescriptorTable(0, basicHeap_->GetGPUDescriptorHandleForHeapStart());

		// レンダーターゲットの設定
		CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(rtvHeap_->GetCPUDescriptorHandleForHeapStart(), frameIndex, device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV));
		auto dsvHandle = dsvHeap_->GetCPUDescriptorHandleForHeapStart();
		commandList_->OMSetRenderTargets(1, &rtvHandle, true, &dsvHandle);
		float clearColor[] = { 1.0f, 1.0f, 0.0f, 1.0f };  // 黄色
		commandList_->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
		commandList_->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

		// 描画処理の設定
		commandList_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST); // プリミティブトポロジの設定 (三角ポリゴン)
		commandList_->IASetVertexBuffers(0, 1, &vertexBufferView_);                // 頂点バッファ
		commandList_->IASetIndexBuffer(&indexBufferView_);                         // インデックスバッファ
		commandList_->DrawIndexedInstanced(fbxVertexInfo_.indices.size(), 1, 0, 0, 0);                        // 描画

		// リソースバリアの設定 (RENDER_TARGET -> PRESENT)
		auto endResourceBarrier = CD3DX12_RESOURCE_BARRIER::Transition(renderTargets_[frameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
		commandList_->ResourceBarrier(1, &endResourceBarrier);

		// 命令のクローズ
		commandList_->Close();
	}


	// コマンドリストの実行
	{
		ID3D12CommandList* commandLists[] = { commandList_.Get() };
		commandQueue_->ExecuteCommandLists(1, commandLists);
		// 画面のスワップ
		ThrowIfFailed(swapchain_->Present(1, 0));
	}

	// GPU処理の終了を待機
	{
		ThrowIfFailed(commandQueue_->Signal(fence_.Get(), ++fenceValue_));
		if (fence_->GetCompletedValue() < fenceValue_) {
			ThrowIfFailed(fence_->SetEventOnCompletion(fenceValue_, fenceEvent_));
			WaitForSingleObject(fenceEvent_, INFINITE);
		}
	}
}

// 終了処理
void DXApplication::OnDestroy()
{
	CloseHandle(fenceEvent_);
}

// D3D12Deviceの生成
void DXApplication::CreateD3D12Device(IDXGIFactory6* dxgiFactory, ID3D12Device** d3d12device)
{
	ID3D12Device* tmpDevice = nullptr;

	// グラフィックスボードの選択
	std::vector <IDXGIAdapter*> adapters;
	IDXGIAdapter* tmpAdapter = nullptr;
	for (int i = 0; SUCCEEDED(dxgiFactory->EnumAdapters(i, &tmpAdapter)); ++i)
	{
		adapters.push_back(tmpAdapter);
	}
	for (auto adapter : adapters)
	{
		DXGI_ADAPTER_DESC adapterDesc;
		adapter->GetDesc(&adapterDesc);
		// AMDを含むアダプターオブジェクトを探して格納（見つからなければnullptrでデフォルト）
		// 製品版の場合は、オプション画面から選択させて設定する必要がある
		std::wstring strAdapter = adapterDesc.Description;
		if (strAdapter.find(L"AMD") != std::string::npos)
		{
			tmpAdapter = adapter;
			break;
		}
	}

	// Direct3Dデバイスの初期化
	D3D_FEATURE_LEVEL levels[] = {
		D3D_FEATURE_LEVEL_12_1,
		D3D_FEATURE_LEVEL_12_0,
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
	};
	for (auto level : levels) {
		if (D3D12CreateDevice(tmpAdapter, level, IID_PPV_ARGS(&tmpDevice)) == S_OK) {
			break; // 生成可能なバージョンが見つかったらループを打ち切り
		}
	}
	*d3d12device = tmpDevice;
}

void DXApplication::ThrowIfFailed(HRESULT hr)
{
	if (FAILED(hr))
	{
		// hrのエラー内容をthrowする
		char s_str[64] = {};
		sprintf_s(s_str, "HRESULT of 0x%08X", static_cast<UINT>(hr));
		std::string errMessage = std::string(s_str);
		throw std::runtime_error(errMessage);
	}
}

void DXApplication::ThrowMessage(std::string message)
{
	throw std::runtime_error(message);
}
