#pragma once

enum class eShaderType : uint8_t
{
    Pixel,
    Vertex,
    Geometry,
    Hull,
    Domain,
    Compute,
    Invalid = 0xFF,
};

static const char* s_dxShaderTypeNames[] = {
    "PIXEL",
    "VERTEX",
    "GEOMETRY",
    "HULL",
    "DOMAIN",
    "COMPUTE",
};

static const char* s_dxShaderTypeShortNames[] = {
    "ps",
    "vs",
    "gs",
    "hs",
    "ds",
    "cs",
};

FORCEINLINE eShaderType GetShaderTypeByName(const std::string& name)
{
    for (int i = 0; i < ARRAYSIZE(s_dxShaderTypeNames); ++i)
    {
        if (!_stricmp(s_dxShaderTypeNames[i], name.c_str()))
            return static_cast<eShaderType>(i);
    }

    return eShaderType::Invalid;
}

struct ShaderAssetHeader_v8_t
{
    PagePtr_t name; // const char*
    eShaderType type;

    char unk_9[3];
    int unk_C; // some count of sorts

    PagePtr_t unk_10; // void*
    PagePtr_t shaderInputFlags; // int64*
};

struct ShaderAssetHeader_v12_t
{
    PagePtr_t name; // const char*
    eShaderType type;

    char shaderFeatures[7];

    PagePtr_t unk_10; // void*
    PagePtr_t shaderInputFlags; // int64*
};

struct ShaderByteCode_t
{
    PagePtr_t data;
    uint32_t dataSize;

    uint32_t unk;

    // only exists in vertex shaders. shader writing code handles this by forcing the size to be 16 bytes (instead of 24)
    // on non-vertex shaders
    PagePtr_t inputSignatureBlob;
};

static_assert(sizeof(ShaderByteCode_t) == 0x18);