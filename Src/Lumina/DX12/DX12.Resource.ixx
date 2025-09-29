// [Difference between resources and resource views]
// Resource: data to be written/read, e.g. a texture or a buffer
// View: data formatted in certain ways so as to be interpreted properly by the graphic drivers
// Reference: https://stackoverflow.com/questions/56821782/what-is-the-difference-between-a-resource-and-a-resource-view

//////	//////	//////	//////	//////	//////
//////	//////	//////	//////	//////	//////
//////	//////	//////	//////	//////	//////

module;

#include<d3d12.h>
#include<dxgi1_6.h>

//////	//////	//////	//////	//////	//////
//////	//////	//////	//////	//////	//////
//////	//////	//////	//////	//////	//////

export module Lumina.DX12 : Resource;

export import : Resource.Commons;

export import : Resource.Buffer;
export import : Resource.Texture2D;