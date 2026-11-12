# Windows renderers

Choose **F1 > Video > Renderer**, save the selection and restart the game. The menu
offers Vulkan, OpenGL and DirectX 12. It also reports the currently active
driver. An unavailable saved choice tries Vulkan, then OpenGL.

| Setting | API | Runtime |
| --- | --- | --- |
| 0 | Vulkan | SDL GPU with SPIR-V |
| 1 | OpenGL | OpenGL 3.3 renderer |
| 2 | DirectX 12 | SDL GPU with DXIL |

The Windows menu exposes only these three APIs. Shader generators and internal
compatibility code remain source components, not additional selectable modes.
The Tag Team render path respects the user's selected scale. Online simulation
speed is independent of that scale and is set to 114%.

DXIL generation uses `python port/tools/dx12_shaders.py --generate`, with the
documented SPIRV-Cross and DXC build tools. Player packages include generated
shaders and do not require a shader compiler.

Performance depends on the scene, selected scale, driver and hardware. Hidden-window
gameplay tests cover simulation and synchronization. Assess frame pacing
separately during visible gameplay on the target hardware.
