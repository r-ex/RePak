#pragma once

/////////////////////////////////////////////
// D3D11_DEPTH_STENCIL_DESC creation flags //
/////////////////////////////////////////////

// DepthEnable
#define DF_DEPTH_ENABLE 0x1

// DepthFunc
#define DF_COMPARISON_NEVER   0b0000
#define DF_COMPARISON_LESS    0b0010
#define DF_COMPARISON_EQUAL   0b0100
#define DF_COMPARISON_GREATER 0b1000

#define DF_COMPARISON_LESS_EQUAL (DF_COMPARISON_EQUAL   | DF_COMPARISON_LESS )    // 0b0110
#define DF_COMPARISON_NOT_EQUAL  (DF_COMPARISON_GREATER | DF_COMPARISON_LESS )    // 0b1010
#define DF_COMPARISON_GREATER_EQUAL (DF_COMPARISON_GREATER | DF_COMPARISON_EQUAL) // 0b1100
#define DF_COMPARISON_ALWAYS (DF_COMPARISON_GREATER | DF_COMPARISON_EQUAL | DF_COMPARISON_LESS) // 0b1110

// DepthWriteMask
#define DF_DEPTH_WRITE_MASK_ALL  0b10000

// StencilEnable
#define DF_STENCIL_ENABLE (1 << 7)


// D3D11_STENCIL_OP
//desc.FrontFace.StencilFailOp = ((visFlags >> 8) & 7) + 1;
//desc.FrontFace.StencilDepthFailOp = ((visFlags >> 11) & 7) + 1;
//desc.FrontFace.StencilPassOp = ((visFlags >> 14) & 7) + 1;



//////////////////////////////////////////
// D3D11_RASTERIZER_DESC creation flags //
//////////////////////////////////////////
// https://learn.microsoft.com/en-us/windows/win32/api/d3d11/ns-d3d11-d3d11_rasterizer_desc


#define RF_FILL_WIREFRAME 0x1

#define RF_CULL_NONE  0b0010
#define RF_CULL_FRONT 0b0100
#define RF_CULL_BACK (RF_CULL_FRONT | RF_CULL_NONE)

// If this parameter is TRUE, a triangle will be considered front-facing if its vertices are counter-clockwise on the render target
// and considered back-facing if they are clockwise.
// If this parameter is FALSE, the opposite is true.
#define RF_INVERT_FACE_DIR 0b1000 // sets FrontCounterClockwise

// used to define values of DepthBias, DepthBiasClamp, SlopeScaledDepthBias
#define RF_PRESET_DECAL     0x10
#define RF_PRESET_SHADOWMAP 0x20
#define RF_PRESET_TIGHTSHADOWMAP 0x40
#define RF_PRESET_UI        0x50
#define RF_PRESET_ZFILL     0x60

// Enable scissor-rectangle culling. All pixels outside an active scissor rectangle are culled.
#define RF_SCISSOR_ENABLE 0x80

