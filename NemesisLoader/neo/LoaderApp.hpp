#include <windows.h>

// Nemesis::LoaderApp
// Базовый класс приложения лоадера
// Приложение, вызывающееся при запуске Loader.exe
namespace Nemesis::LoaderApp
{
	bool ShouldClose;
	// Создаёт все нужные для приложения ресурсы.
	bool LoaderInit(WNDCLASSEXW wc, bool coInitialized);
	void CreateResources();

	bool CreateDeviceD3D(HWND hWnd);
	void CleanupDeviceD3D();
	
	void CreateRenderTarget();
	void CleanupRenderTarget();

	void UpdateSceneCapture();
	bool EnsureSceneCapture(UINT w, UINT h);
	void CleanupSceneCapture();
}