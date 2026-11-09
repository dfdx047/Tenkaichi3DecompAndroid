#pragma once
#include "gs/gpu/hd_texture_pack.h"
#include "image.hpp"

// The GPU image of a loaded replacement (Granite), for the code that hands it to the renderer.
struct HdTexturePack::Image {Vulkan::ImageHandle handle;};
