#pragma once

#include <d3d11.h>
#include <filesystem>
#include <string>
#include <vector>

bool LoadTextureFromFile(const wchar_t* filename, ID3D11Device* device,
    ID3D11ShaderResourceView** out_srv,
    int* out_width = nullptr, int* out_height = nullptr);

void ReleaseTexture(ID3D11ShaderResourceView*& texture);

bool ReadFileToString(const std::string& path, std::string& out);
bool FileExists(const std::string& path);
bool DirectoryExists(const std::string& path);
std::vector<std::string> ListDirectories(const std::string& path);
std::string CombinePath(const std::string& a, const std::string& b);

std::filesystem::path GetExecutableDirectory();