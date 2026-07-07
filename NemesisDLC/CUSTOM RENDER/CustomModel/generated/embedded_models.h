#pragma once

namespace Nemesis { namespace EmbeddedModels {

    struct FileEntry { const char* path; unsigned int offset; unsigned int size; };

    extern const unsigned char* const g_chunks[];
    extern const unsigned int g_chunkSize;
    extern const unsigned int g_chunkCount;
    extern const unsigned int g_totalSize;
    extern const FileEntry g_files[];
    extern const unsigned int g_fileCount;

    // Resolves the csgo content root from client.dll and extracts all embedded
    // model files there (creating dirs, skipping files already present with same size).
    void Deploy();

}}
