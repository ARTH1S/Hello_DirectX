#include "Engine.h"


int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR lpCmdLine,
	_In_ int nCmdShow)
{
	HRESULT hr = CoInitialize(NULL);
	if (FAILED(hr))
	{
		ErrorLogger::Log(hr, "Failed to call CoInitialize.");
		return -1;
	}

	Engine engine;
	if (engine.Initialize(hInstance, "TEST", "wclass", 640, 480))
	{
		while (engine.ProcessMessages() == true) // тру пока окно существует. фолс - окно уничтожено
		{
			engine.Update();
			engine.RenderFrame();
		}
	}
	return 0;
}

//INPUT ASSEMBLER
//VETEX SHADER+
//RASTERIZER
//PIXEL SHADER+
//OUTPUT MERGER

//CREATE OUR VERTEX BUFFER
//DRAW