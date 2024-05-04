#include "MyD3d11.h"

void MyD3d11::SafeRelease(auto*& pData)
{
	if (pData)
	{
		pData->Release();
		pData = nullptr;
	}
}

bool MyD3d11::WorldToScreen(Vector3 pWorldPos, Vector3& pScreenPos, float* pMatrix, int pWinWidth, int pWinHeight)
{
	float matrix[4][4];

	memcpy(matrix, pMatrix, 16 * sizeof(float));

	const float midWidth { static_cast<float>(pWinWidth) / 2.0f };
	const float midHeight { static_cast<float>(pWinHeight) / 2.0f };

	const float w {
		matrix[0][3] * pWorldPos.x +
		matrix[1][3] * pWorldPos.y +
		matrix[2][3] * pWorldPos.z +
		matrix[3][3] };

	if (w < 0.65f) return false;

	const float x {
		matrix[0][0] * pWorldPos.x +
		matrix[1][0] * pWorldPos.y +
		matrix[2][0] * pWorldPos.z +
		matrix[3][0] };

	const float y {
		matrix[0][1] * pWorldPos.x +
		matrix[1][1] * pWorldPos.y +
		matrix[2][1] * pWorldPos.z +
		matrix[3][1] };

	pScreenPos.x = (midWidth  + midWidth  * x / w);
	pScreenPos.y = (midHeight - midHeight * y / w);
	pScreenPos.z = 0;

	return true;
}

bool MyD3d11::SetInputLayout(ID3D10Blob* pCompiledShaderBlob)
{
	const D3D11_INPUT_ELEMENT_DESC layout[2] = {
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0}
	};

	const UINT numElements{ ARRAYSIZE(layout) };

	const HRESULT hRes{ m_device->CreateInputLayout(layout, numElements, pCompiledShaderBlob->GetBufferPointer(), pCompiledShaderBlob->GetBufferSize(), &m_vInputLayout) };

	if (FAILED(hRes)) return false;

	return true;
}

void MyD3d11::SetOrthoMatrix(D3D11_VIEWPORT pViewport)
{
	m_orthoMatrix = DirectX::XMMatrixOrthographicOffCenterLH(0, pViewport.Width, pViewport.Height, 0, 0.0f, 1.0f);
}

bool MyD3d11::CompileShader(const char* szShader, const char* szEntrypoint, const char* szTarget, ID3D10Blob** compiledShaderBlob)
{
	ID3D10Blob* pErrorBlob{ nullptr };

	const auto hRes{ D3DCompile(szShader, strlen(szShader), 0, nullptr, nullptr, szEntrypoint, szTarget, D3DCOMPILE_ENABLE_STRICTNESS, 0, compiledShaderBlob, &pErrorBlob) };
	if (FAILED(hRes))
	{
		if (pErrorBlob)
		{
			char szError[256]{ 0 };
			memcpy(szError, pErrorBlob->GetBufferPointer(), pErrorBlob->GetBufferSize());
			MessageBoxA(nullptr, szError, "Error", MB_OK);
		}
		return false;
	}
	return true;
}

bool MyD3d11::CompileShaders()
{
	ID3D10Blob* compiledShaderBlob{ nullptr };

	// Vertex shader
	if (!CompileShader(shaders, "VS", "vs_5_0", &compiledShaderBlob))
		return false;

	HRESULT hRes{ m_device->CreateVertexShader(compiledShaderBlob->GetBufferPointer(), compiledShaderBlob->GetBufferSize(), nullptr, &m_vertexShader) };
	if (FAILED(hRes)) return false;

	// Sets the input layout of the vertex buffer
	if (!SetInputLayout(compiledShaderBlob))
		return false;

	SafeRelease(compiledShaderBlob);

	// Pixel shader
	if (!CompileShader(shaders, "PS", "ps_5_0", &compiledShaderBlob))
		return false;

	hRes = m_device->CreatePixelShader(compiledShaderBlob->GetBufferPointer(), compiledShaderBlob->GetBufferSize(), nullptr, &m_pixelShader);
	if (FAILED(hRes)) return false;

	return true;
}

bool MyD3d11::SetViewport()
{
	UINT numViewports{ D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE };

	m_context->RSGetViewports(&numViewports, m_viewports);

	if (!numViewports || !m_viewports[0].Width)
	{
		m_hwnd = MyD3dUtils::FindMainWindow(GetCurrentProcessId());

		if (!GetClientRect(m_hwnd, &m_hRect))
			return false;

		// Setup viewport
		m_viewport.Width = static_cast<FLOAT>(m_hRect.right);
		m_viewport.Height = static_cast<FLOAT>(m_hRect.bottom);
		m_viewport.TopLeftX = static_cast<FLOAT>(m_hRect.left);
		m_viewport.TopLeftY = static_cast<FLOAT>(m_hRect.top);
		m_viewport.MinDepth = 0.0f;
		m_viewport.MaxDepth = 1.0f;

		return true;
	}

	m_viewport = m_viewports[0];

	return true;
}

bool MyD3d11::SetConstantBuffer()
{
	SetOrthoMatrix(m_viewport);

	D3D11_BUFFER_DESC buffer_desc{ 0 };
	buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	buffer_desc.ByteWidth = sizeof(DirectX::XMMATRIX);
	buffer_desc.Usage = D3D11_USAGE_DEFAULT;

	D3D11_SUBRESOURCE_DATA subResourceLine{ 0 };
	subResourceLine.pSysMem = &m_orthoMatrix;

	HRESULT hRes{ m_device->CreateBuffer(&buffer_desc, &subResourceLine, &m_constantBuffer) };
	if (FAILED(hRes)) return false;

	return true;
}

bool MyD3d11::SetDeviceContext(IDXGISwapChain* pSwapchain)
{
	// Get game device
	HRESULT hRes{ pSwapchain->GetDevice(__uuidof(ID3D11Device), reinterpret_cast<void**>(&m_device)) };
	if (FAILED(hRes)) return false;

	m_device->GetImmediateContext(&m_context);

	m_context->OMGetRenderTargets(1, &m_renderTargetView, nullptr);

	// renderTargetView backup
	if (!m_renderTargetView)
	{
		ID3D11Texture2D* backbuffer{ nullptr };
		hRes = pSwapchain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&backbuffer));
		if (FAILED(hRes)) return false;

		hRes = m_device->CreateRenderTargetView(backbuffer, nullptr, &m_renderTargetView);
		backbuffer->Release();
		if (FAILED(hRes)) return false;
	}

	return true;
}

bool MyD3d11::InitDraw(IDXGISwapChain* pSwapchain)
{
	m_swapChain = pSwapchain;

	if (!SetDeviceContext(m_swapChain)) return false;

	if (!CompileShaders()) return false;

	if (!SetViewport()) return false;

	if (!SetConstantBuffer()) return false;

	return true;
}

bool MyD3d11::SetPresent()
{
	DXGI_SWAP_CHAIN_DESC sd{ 0 };
	sd.BufferCount = 1;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.OutputWindow = MyD3dUtils::FindMainWindow(GetCurrentProcessId());
	sd.Windowed = TRUE;
	sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
	sd.SampleDesc.Count = 1;

	ID3D11Device* pDevice{ nullptr };
	IDXGISwapChain* pSwapchain{ nullptr };

	const HRESULT hRes{ D3D11CreateDeviceAndSwapChain(
		nullptr,
		D3D_DRIVER_TYPE_HARDWARE,
		nullptr,
		0,
		nullptr,
		0,
		D3D11_SDK_VERSION,
		&sd,
		&pSwapchain,
		&pDevice,
		nullptr,
		nullptr) };

	if (FAILED(hRes)) return false;

	void** pVMT = *(void***)pSwapchain;

	// Get Present's address out of vmt
	o_present = (TPresent)(pVMT[(UINT)IDXGISwapChainVMT::Present]);

	SafeRelease(pDevice);

	return true;
}

void MyD3d11::BeginDraw()
{
	SetViewport();
	SetConstantBuffer();
}

void MyD3d11::SetInputAssembler(D3D_PRIMITIVE_TOPOLOGY pPrimitiveTopology)
{
	const UINT stride{ sizeof(Vertex) };
	const UINT offset{ 0 };

	m_context->IASetVertexBuffers(0, 1, &m_vertexBuffer, &stride, &offset);
	m_context->IASetInputLayout(m_vInputLayout);
	m_context->IASetPrimitiveTopology(pPrimitiveTopology);
}

void MyD3d11::SetLineVBuffer(float x, float y, float x2, float y2, D3DCOLORVALUE pColor)
{
	Vertex vertices[2] = {
	{ DirectX::XMFLOAT3(x, y, 1.0f), pColor },
	{ DirectX::XMFLOAT3(x2, y2, 1.0f),	pColor },
	};

	D3D11_BUFFER_DESC buffer_desc_line{ 0 };
	buffer_desc_line.BindFlags = D3D11_BIND_INDEX_BUFFER;
	buffer_desc_line.ByteWidth = sizeof(Vertex) * 2;
	buffer_desc_line.Usage = D3D11_USAGE_DEFAULT;

	D3D11_SUBRESOURCE_DATA subResourceLine{ 0 };
	subResourceLine.pSysMem = &vertices;

	m_device->CreateBuffer(&buffer_desc_line, &subResourceLine, &m_vertexBuffer);
}

void MyD3d11::DrawLine(float x, float y, float x2, float y2, D3DCOLORVALUE pColor)
{
	// Setup vertices
	SetLineVBuffer(x, y, x2, y2, pColor);

	// Input Assembler
	SetInputAssembler(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);

	// Vertex Shader
	m_context->VSSetConstantBuffers(0, 1, &m_constantBuffer);
	m_context->VSSetShader(m_vertexShader, nullptr, 0);

	// Pixel Shader
	m_context->PSSetShader(m_pixelShader, nullptr, 0);

	// Output Merger
	//m_context->OMSetRenderTargets(1, &m_renderTargetView, nullptr);

	// Rasterizer
	m_context->RSSetViewports(1, &m_viewport);

	m_context->Draw(2, 0);
}

void MyD3d11::SetLineWHVBuffer(float pX, float pY, float pWidth, float pHeight, D3DCOLORVALUE pColor)
{
	Vertex vertices[2] = {
		{ DirectX::XMFLOAT3(pX,			   pY,			 1.0f),	pColor },
		{ DirectX::XMFLOAT3(pX + pWidth,   pY + pHeight, 1.0f),	pColor },
	};

	D3D11_BUFFER_DESC buffer_desc_line{ 0 };
	buffer_desc_line.BindFlags = D3D11_BIND_INDEX_BUFFER;
	buffer_desc_line.ByteWidth = sizeof(Vertex) * 2;
	buffer_desc_line.Usage = D3D11_USAGE_DEFAULT;

	D3D11_SUBRESOURCE_DATA subResourceLine{ 0 };
	subResourceLine.pSysMem = &vertices;

	m_device->CreateBuffer(&buffer_desc_line, &subResourceLine, &m_vertexBuffer);
}

void MyD3d11::DrawLineWH(float pX, float pY, float pWidth, float pHeight, D3DCOLORVALUE pColor)
{
	// Setup vertices
	SetLineWHVBuffer(pX,  pY, pWidth, pHeight, pColor);

	// Input Assembler
	SetInputAssembler(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);

	// Vertex Shader
	m_context->VSSetConstantBuffers(0, 1, &m_constantBuffer);
	m_context->VSSetShader(m_vertexShader, nullptr, 0);

	// Pixel Shader
	m_context->PSSetShader(m_pixelShader, nullptr, 0);
	
	// Output Merger
	//m_context->OMSetRenderTargets(1, &m_renderTargetView, nullptr);

	// Rasterizer
	m_context->RSSetViewports(1, &m_viewport);

	m_context->Draw(2, 0);
}

void MyD3d11::SetBoxVBuffer(float pX, float pY, float pWidth, float pHeight, D3DCOLORVALUE pColor)
{
	Vertex vertices[5] = {
		{ DirectX::XMFLOAT3(pX,				pY,			  1.0f),		pColor },
		{ DirectX::XMFLOAT3(pX + pWidth,	pY,			  1.0f),		pColor },
		{ DirectX::XMFLOAT3(pX + pWidth,	pY + pHeight, 1.0f),		pColor },
		{ DirectX::XMFLOAT3(pX,				pY + pHeight, 1.0f),		pColor },
		{ DirectX::XMFLOAT3(pX,				pY,			  1.0f),		pColor }
	};

	D3D11_BUFFER_DESC buffer_desc_line{ 0 };
	buffer_desc_line.BindFlags = D3D11_BIND_INDEX_BUFFER;
	buffer_desc_line.ByteWidth = sizeof(Vertex) * 5;
	buffer_desc_line.Usage = D3D11_USAGE_DEFAULT;

	D3D11_SUBRESOURCE_DATA subResourceLine{ 0 };
	subResourceLine.pSysMem = &vertices;

	m_device->CreateBuffer(&buffer_desc_line, &subResourceLine, &m_vertexBuffer);
}

void MyD3d11::DrawBox(float pX, float pY, float pWidth, float pHeight, D3DCOLORVALUE pColor)
{
	// Setup vertices TL, TR, BR, BL
	SetBoxVBuffer(pX, pY, pWidth, pHeight, pColor);
	
	// Input Assembler
	SetInputAssembler(D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP);

	// Vertex Shader
	m_context->VSSetConstantBuffers(0, 1, &m_constantBuffer);
	m_context->VSSetShader(m_vertexShader, nullptr, 0);

	// Pixel Shader
	m_context->PSSetShader(m_pixelShader, nullptr, 0);

	// Output Merger
	//m_context->OMSetRenderTargets(1, &m_renderTargetView, nullptr);

	// Rasterizer
	m_context->RSSetViewports(1, &m_viewport);

	m_context->Draw(5, 0);
}

void MyD3d11::TestRender()
{
	//this easily lets you debug viewport and ortho pMatrix
	//it will draw from top left to bottom right if your viewport is correctly setup
	DrawLine(0, 0, 640, 360, green);
	DrawLine(0, 0, 640, -360, red);
	DrawLine(0, 0, -640, 360, magenta);
	DrawLine(0, 0, -640, -360, yellow);
	DrawBox(0, 0, 50, 100, green); // TL, TR, BR, BL
}

MyD3d11::~MyD3d11()
{
	SafeRelease(m_constantBuffer);
	SafeRelease(m_vertexBuffer);
	SafeRelease(m_vInputLayout);
	SafeRelease(m_vertexShader);
	SafeRelease(m_pixelShader);
}