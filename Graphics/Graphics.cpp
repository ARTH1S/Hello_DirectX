#include "Graphics.h"

bool Graphics::Initialize(HWND hwnd, int width, int height)
{
	windowWidth = width;
	windowHeight = height;
	fpsTimer.Start();

	if (!InitializeDirectX(hwnd ))
		return false;
	
	if(!InitializeShaders())
		return false;

	if (!InitializeScene())
		return false;

	//SETUP IMGUI
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	ImGui_ImplWin32_Init(hwnd);
	ImGui_ImplDX11_Init(device.Get(), deviceContext.Get());
	ImGui::StyleColorsDark();

	return true;
}

void Graphics::RenderFrame()
{
	float bgcolor[] = { 0.0f, 0.0f, 0.0f, 1.0 };
	deviceContext->ClearRenderTargetView(renderTargetView.Get(), bgcolor);
	this->deviceContext->ClearDepthStencilView(this->depthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

	deviceContext->IASetInputLayout(this->vertexshader.GetInputLayout());
	deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY::D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	deviceContext->RSSetState(rasterizerState.Get());

	deviceContext->OMSetDepthStencilState(depthStencilState.Get(), 0);

	deviceContext->PSGetSamplers(0, 1, this->samplerState.GetAddressOf());

	deviceContext->VSSetShader(vertexshader.GetShader(), NULL, 0);
	deviceContext->PSSetShader(pixelshader.GetShader(), NULL, 0);

	UINT offset = 0;

	//UPDATE CONSTANT BUFFER>
	static float translationOffset[3] = { 0, 0, 0 };
	XMMATRIX world = XMMatrixTranslation(translationOffset[0], translationOffset[1], translationOffset[2]);
	constantBuffer.data.mat = world * camera.GetViewMatrix() * camera.GetProjectionMatrix();
	constantBuffer.data.mat = DirectX::XMMatrixTranspose(constantBuffer.data.mat);

	{
		/*static XMVECTOR eyePos = DirectX::XMVectorSet(0.0f, -4.0f, -2.0f, 0.0f);
		XMFLOAT3 eyePosFloat3;
		XMStoreFloat3(&eyePosFloat3, eyePos);
		eyePosFloat3.y += 0.01f;
		eyePos = DirectX::XMLoadFloat3(&eyePosFloat3);*/
		//static DirectX::XMVECTOR lookAtPos = DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f); //LOOK AT CENTRE OF THE WORLD
		//static DirectX::XMVECTOR upVector = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f); //POSITIVE Y ASIX = UP
		//DirectX::XMMATRIX viewMatrix = DirectX::XMMatrixLookAtLH(eyePos, lookAtPos, upVector);
		//float fovDegrees = 59.0f; //DEGREE FOR //Perspective matrix is taking in an FOV value that is in VERTICAL FOV format, so if you really wanted 90 degrees horizontal you'd want to set (or convert) that value to 59, not 90
		//float forRadians = (fovDegrees / 360.0f) * DirectX::XM_2PI;
		//float aspectRatio = static_cast<float>(windowWidth) / static_cast<float>(windowHeight);
		//float nearZ = 0.1f;
		//float farZ = 1000.0f;
		
		//DirectX::XMMATRIX projectionMatrix = DirectX::XMMatrixPerspectiveFovLH(forRadians, aspectRatio, nearZ,farZ);
	}
	if (!constantBuffer.ApplyChanges())
		return;
	deviceContext->VSSetConstantBuffers(0, 1, constantBuffer.GetAddressOf());

	//SQUARE
	deviceContext->PSSetShaderResources(0, 1, myTexture.GetAddressOf()); //its set "TEXTURE" in pixelshader
	this->deviceContext->IASetVertexBuffers(0, 1, vertexBuffer.GetAddressOf(), vertexBuffer.StridePtr(), &offset);
	deviceContext->IASetIndexBuffer(indicesBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
	deviceContext->DrawIndexed(indicesBuffer.BufferSize(), 0, 0);

	//Draw text
	static int fpsCounter = 0;
	static std::string fpsString = "FPS: 0";
	fpsCounter += 1;
	if (fpsTimer.GetMilisecondsElapsed() > 1000.0)
	{
		fpsString = "FPS: " + std::to_string(fpsCounter);
		fpsCounter = 0;
		fpsTimer.Restart();

	}
	spriteBatch->Begin();
	spriteFont->DrawString(spriteBatch.get(),	//ADRESS SPRITEBATCH
		StringConverter::StringToWide(fpsString).c_str(),							//PASS TEXT
		DirectX::XMFLOAT2(0.0f, 0.0f),			//POSITION
		DirectX::Colors::White,					//COLOR
		0.0f,									//ROTATION
		DirectX::XMFLOAT2(0.0f, 0.0f),			//ORIGIN COORDINAT(start)
		DirectX::XMFLOAT2(1.0f, 1.0f));			//SCALE
		
	spriteBatch->End();



	static int counter = 0;
	//Start the Dear ImGui frame
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
	//Create ImGui Test Window
	ImGui::Begin("Test");
	ImGui::Text("This is example text");
	while (ImGui::Button("CLICK ME"))
	{
		counter += 1;
	}
	ImGui::SameLine();
	std::string clickCount = "Click Count: " + std::to_string(counter);
	ImGui::Text(clickCount.c_str());
	ImGui::DragFloat3("Translation X/Y/Z", translationOffset, 0.1f, -5.0f, 5.0f);
	ImGui::End();
	//Asseble Together Draw Data
	ImGui::Render();
	//Render Draw Data
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

	swapchain->Present(1, NULL); //1 - VSink ON 2 - OFF
}

bool Graphics::InitializeDirectX(HWND hwnd)
{
	std::vector<AdapterData> adapters = AdapterReader::GetAdapters();

	if (adapters.size() < 1)
	{
		ErrorLogger::Log("No DXGI Adapters found.");
		return false;
	}

	DXGI_SWAP_CHAIN_DESC  scd;
	ZeroMemory(&scd, sizeof(DXGI_SWAP_CHAIN_DESC));

	scd.BufferDesc.Width = windowWidth;
	scd.BufferDesc.Height = windowHeight; 
	scd.BufferDesc.RefreshRate.Numerator = 60;
	scd.BufferDesc.RefreshRate.Denominator = 1;
	scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	scd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED; 
	scd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;

	scd.SampleDesc.Count = 1;
	scd.SampleDesc.Quality = 0;

	scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	scd.BufferCount = 1;
	scd.OutputWindow = hwnd;
	scd.Windowed = TRUE;
	scd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
	scd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	HRESULT hr;
	hr = D3D11CreateDeviceAndSwapChain(	adapters[0].pAdapter,					// IDXGI Adapter
										D3D_DRIVER_TYPE_UNKNOWN, 
										NULL,									// FOR SOFTWARE DRIVER TUPE
										NULL,									// FLAGS FOR RUNTIME LAYERS
										NULL,									// FEATURE LEVELS ARRAY
										0,										//# OF FEATURE LEVELS IN ARRAY
										D3D11_SDK_VERSION,
										&scd,									//SWAPCHAIN DESCRIPTION
										this->swapchain.GetAddressOf(),			//SWAPCHAIN CONTEXT ADRESS
										this->device.GetAddressOf(),			//DEVICE ADRESS
										NULL,									// SUPPORTED FEATURE LEVEL
										this->deviceContext.GetAddressOf());	//DEVICE CONTEXT ADRESS

	if (FAILED(hr))
	{
		ErrorLogger::Log(hr, "Failed to create device and swapchain.");
		return false;
	}

	/***** POINTER TO THE BACKBUFFER ******/
	Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer;
	hr = this->swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(backBuffer.GetAddressOf()));
	if (FAILED(hr)) //If error occurred
	{
		ErrorLogger::Log(hr, "GetBuffer Failed.");
		return false;
	}

	hr = device->CreateRenderTargetView(backBuffer.Get(),						//POINTER TO BACKBUFFER
										NULL,
										renderTargetView.GetAddressOf());		//ADRESS TO THE POINTER FOR OUR TARGET RENDER VIEW
	if (FAILED(hr)) //If error occurred
	{
		ErrorLogger::Log(hr, "Failed to create render target view.");
		return false;
	}

	//Describe our Depth/Stencil Buffer
	D3D11_TEXTURE2D_DESC depthStencilDesc;
	depthStencilDesc.Width = windowWidth;
	depthStencilDesc.Height = windowHeight;
	depthStencilDesc.MipLevels = 1;
	depthStencilDesc.ArraySize = 1;
	depthStencilDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	depthStencilDesc.SampleDesc.Count = 1;
	depthStencilDesc.SampleDesc.Quality = 0;
	depthStencilDesc.Usage = D3D11_USAGE_DEFAULT;
	depthStencilDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	depthStencilDesc.CPUAccessFlags = 0;
	depthStencilDesc.MiscFlags = 0;

	hr = this->device->CreateTexture2D(&depthStencilDesc, NULL, this->depthStencilBuffer.GetAddressOf());
	if (FAILED(hr)) //If error occurred
	{
		ErrorLogger::Log(hr, "Failed to create depth stencil buffer.");
		return false;
	}

	hr = this->device->CreateDepthStencilView(this->depthStencilBuffer.Get(), NULL, this->depthStencilView.GetAddressOf());
	if (FAILED(hr)) //If error occurred
	{
		ErrorLogger::Log(hr, "Failed to create depth stencil view.");
		return false;
	}

	deviceContext->OMSetRenderTargets(1, this->renderTargetView.GetAddressOf(), depthStencilView.Get()); //FOR OUTPUT MERGER AND SETTING WHERE WE ARE GOING TO RENDER

	//Create depth stencil state
	D3D11_DEPTH_STENCIL_DESC depthstencildesc;
	ZeroMemory(&depthstencildesc, sizeof(D3D11_DEPTH_STENCIL_DESC));

	depthstencildesc.DepthEnable = true;
	depthstencildesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK::D3D11_DEPTH_WRITE_MASK_ALL;
	depthstencildesc.DepthFunc = D3D11_COMPARISON_FUNC::D3D11_COMPARISON_LESS_EQUAL;

	hr = this->device->CreateDepthStencilState(&depthstencildesc, this->depthStencilState.GetAddressOf());
	if (FAILED(hr))
	{
		ErrorLogger::Log(hr, "Failed to create depth stencil state.");
		return false;
	}

	//Create the Viewport
	D3D11_VIEWPORT viewport; 
	ZeroMemory(&viewport, sizeof(D3D11_VIEWPORT));

	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	viewport.Width = windowWidth;
	viewport.Height = windowHeight;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;

	//Set the Viewport
	this->deviceContext->RSSetViewports(1, &viewport);

	//Create Rasterizer State
	D3D11_RASTERIZER_DESC rasterizerDesc;
	ZeroMemory(&rasterizerDesc, sizeof(D3D11_RASTERIZER_DESC));

	rasterizerDesc.FillMode = D3D11_FILL_MODE::D3D11_FILL_SOLID;
	rasterizerDesc.CullMode = D3D11_CULL_MODE::D3D11_CULL_NONE; //NONE - фиксит в какой стороне смотрит треугольник
	//rasterizerDesc.FrontCounterClockwise = TRUE; //фиксит ошибку в случае если поворот будет не за часовой стрелкой. но теперь можно использовать только против часовой
	hr = this->device->CreateRasterizerState(&rasterizerDesc, this->rasterizerState.GetAddressOf());
	if (FAILED(hr)) 
	{
		ErrorLogger::Log(hr, "Failed to create rasterizer state.");
		return false;
	}

	spriteBatch = std::make_unique<DirectX::SpriteBatch>(this->deviceContext.Get());
	spriteFont = std::make_unique<DirectX::SpriteFont>(this->device.Get(), L"Assets\\Fonts\\comic_sans_ms_16.spritefont");

	//Create sampler description for sampler state
	D3D11_SAMPLER_DESC sampDesc; 
	ZeroMemory(&sampDesc, sizeof(sampDesc));
	sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP; //COORDINAT X ON TEXTURES
	sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;	//COORDINAT Y ON TEXTURES
	sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;	//4D textures. what?
	sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	sampDesc.MinLOD = 0;
	sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
	hr = this->device->CreateSamplerState(&sampDesc, this->samplerState.GetAddressOf()); //Create sampler state
	if (FAILED(hr))
	{
		ErrorLogger::Log(hr, "Failed to create sampler state.");
		return false;
	}

	return true;
}

/************ INPUT ASSEMBLER **************/
bool Graphics::InitializeShaders()
{
	/******* FIND PATH MACRO *******/
	std::wstring shaderFolder;
#pragma region DetermineShaderPath
	if (IsDebuggerPresent() == TRUE)
	{
#ifdef _DEBUG 
#ifdef _WIN64
		shaderFolder = L"..\\Hello_DirectX11\\x64\\Debug\\";
#else 
		shaderFolder = L"..\\Hello_DirectX11\\Debug\\";
#endif
#else 
#ifdef _WIN64
		shaderFolder = L"..\\Hello_DirectX11\\x64\\Release\\";
#else
		shaderFolder = L"..\\Hello_DirectX11\\Release\\";
#endif
#endif
	}
	/******* END FIND PATH MACRO *******/

	D3D11_INPUT_ELEMENT_DESC layout[] =
	{
		{"POSITION", 0, DXGI_FORMAT::DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
		D3D11_INPUT_CLASSIFICATION::D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{"TEXCOORD", 0, DXGI_FORMAT::DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, //т.к. это 2 элемент, передаЄм макрос что бы определить смещение
		D3D11_INPUT_CLASSIFICATION::D3D11_INPUT_PER_VERTEX_DATA, 0 }
	};

	UINT numElement = ARRAYSIZE(layout);

	if (!vertexshader.Initialize(device, shaderFolder + L"VertexShader.cso", layout, numElement))
		return false;

	if (!pixelshader.Initialize(device, shaderFolder + L"PixelShader.cso"))
		return false;

	return true;
}

bool Graphics::InitializeScene()
{
	//Textured Square
	Vertex v[] =
	{		//coordinats      depth	//tri	//ќЅя«ј“≈Ћ№Ќќ „“ќ Ѕџ ЅџЋќ ѕќ „ј—ќ¬ќ… —“–≈Ћ ≈
		Vertex(-0.5f, -0.5f,  0.0f, 0.0f, 1.0f), //Bottomm left - [0]
		Vertex(-0.5f, 0.5f,  0.0f,  0.0f, 0.0f), //Top left		- [1]
		Vertex(	0.5f,  0.5f,  0.0f, 1.0f, 0.0f), //Top right	- [2]
		Vertex(0.5f,  -0.5f,  0.0f, 1.0f, 1.0f), //Bottom right	- [3]
	};

	//LOAD VERTEX DATA
	HRESULT hr = vertexBuffer.Initialize(device.Get(), v, ARRAYSIZE(v));
	if (FAILED(hr))
	{
		ErrorLogger::Log(hr, "Failed to create vertex buffer.");
		return false;
	}

	DWORD indices[] =
	{
		0, 1, 2,
		0, 2, 3
	};

	//Load Index Data

	hr = indicesBuffer.Initialize(device.Get(), indices, ARRAYSIZE(indices));
	if (FAILED(hr))
	{
		ErrorLogger::Log(hr, "Failed to create indices buffer.");
		return hr;
	}

	//LOAD TEXTURE
	hr = DirectX::CreateWICTextureFromFile(this->device.Get(), L"Assets\\Textures\\image.jpg", nullptr, myTexture.GetAddressOf());
	if (FAILED(hr))   
	{
		ErrorLogger::Log(hr, "Failed to create wic texture from file.");
		return false;
	}

	//INITIALIZE CONST BUFFER(S)
	hr = constantBuffer.Initialize(device.Get(), deviceContext.Get());
	if (FAILED(hr))
	{
		ErrorLogger::Log(hr, "Failed to initialize constant buffer.");
		return false;
	}

	camera.SetPosition(0.0f, 0.0f, -2.0f);
	camera.SetProjectionValues(90.0f, static_cast<float>(windowWidth) / static_cast<float>(windowHeight), 0.1f, 1000.0f);

	return true;
}
