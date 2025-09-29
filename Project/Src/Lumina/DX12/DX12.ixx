module;

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

// dxguid.lib			| GUID (Globally Unique Identifier)
#pragma comment(lib, "dxguid.lib")

//////	//////	//////	//////	//////	//////
//////	//////	//////	//////	//////	//////
//////	//////	//////	//////	//////	//////

export module Lumina.DX12;

//****	******	******	******	******	****//

export import <d3d12.h>;
export import <dxgi1_6.h>;

export import : GraphicsDevice;

export import : Command;

export import : Resource;
export import : ImageTexture;
export import : Descriptor;

export import : Shader;
export import : RootSignature;
export import : PipelineState;

export import : FrameBufferSwapChain;

export import : Debug;