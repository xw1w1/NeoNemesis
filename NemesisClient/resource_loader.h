#ifndef NEMESIS_RESOURCE_LOADER_H
#define NEMESIS_RESOURCE_LOADER_H

#include <d3d11.h>
#include <windows.h>

bool LoadTextureFromMemory(const unsigned char* data, unsigned int size,
    ID3D11Device* device, ID3D11ShaderResourceView** out_srv);

bool LoadTextureByName(const char* name, ID3D11Device* device,
    ID3D11ShaderResourceView** out_srv);

const unsigned char* GetResourceBytes(const char* name, unsigned int* out_size);

#endif
