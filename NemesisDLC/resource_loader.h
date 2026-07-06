#include <d3d11.h>
#include <windows.h>
#include <wincodec.h>
#include <vector>

#pragma comment(lib, "windowscodecs.lib")

bool LoadTextureFromResource(int resource_id, ID3D11Device* device,
    ID3D11ShaderResourceView** out_srv);

bool LoadResourceToMemory(int resource_id, std::vector<uint8_t>& out_data);