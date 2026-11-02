# The Vulkan renderer

ScrapC.exe renders with Vulkan only. The Direct3D 8 backend, the device dialog and the REF fallback were removed on 2026-11-02. This file describes what is in `src/game/engine3d/Render/` and `shaders/`.

## Layers

| Path | Content |
|------|---------|
| `include/scrap/rdev.h`, `Render/Rdev.c` | the render device: `Rd_*` functions, the interface the game calls |
| `Render/rdevbackend.h` | one `Vk_*` function for each `Rd_*` function |
| `Render/RdevLog.c` | the D3D log of the tests |
| `Render/vk/RdevVk.c`, `vkobj.h` | the objects of Direct3D 8 on Vulkan resources |
| `Render/vk/VkDraw.c`, `vkdraw.h` | state, pipelines, draws, present |
| `Render/vk/VkCore.c`, `vkcore.h` | instance, device, swapchain, frames, memory |
| `shaders/` | GLSL, compiled to SPIR-V at build time |

## Render device layer

The game was written against Direct3D 8, so `Rd_*` keeps its shape: the first argument is an opaque object (the IDirect3D8, the device, a texture, a surface, a buffer, a D3DX mesh), the other arguments and the return values are those of the COM method. There is one function per method the game uses. `Rd_Direct3DCreate8` returns the object of the backend, or NULL when Vulkan cannot start.

`Rdev.c` forwards every call to the `Vk_*` function of the same name. For device calls it checks first whether the device is the stand-in of the D3D log (`rd_logDevice`). The tests (`SDTest`) switch the log on. `RdevLog.c` then writes each device call as text, with the vertices of draw calls and the values of state calls as hex words, and passes the call to the real device (`rd_logReal`). The test scripts compare these logs between two builds.

Every object starts with `VkoObj` (kind, reference count), so `Rd_AddRef` and `Rd_Release` work on any of them. The device is created with `D3DCREATE_FPU_PRESERVE` by the game, so the x87 state is not touched by the backend.

## The backend objects

`RdevVk.c` emulates one adapter. The device keeps the state Direct3D 8 keeps: 256 render states, 8 texture stages with 32 states each, the transforms (world matrices up to index 256), viewport, material, 8 lights, the stream source, the index buffer, the pixel shader and its 8 constants, and the gamma ramp.

Adapter data comes from Vulkan (`Vk_GpuProperties`): name, vendor and device id, driver version. The driver name is `vulkan-1.dll`. The caps are fixed values of a Direct3D 8 card of its time: 8 lights, 4 simultaneous textures, pixel shader version 1.4, stencil, maximum texture size from `maxImageDimension2D`. Display modes come from `EnumDisplaySettingsA`, 16 bit as R5G6B5 and 32 bit as X8R8G8B8, without refresh rates and without duplicates. If Windows gives no list, the desktop size is used in both formats.

Texture formats map to Vulkan formats with a swizzle (A8, L8, A8L8) or a conversion on upload:

- A8R8G8B8, X8R8G8B8, R5G6B5, A1R5G5B5, X1R5G5B5, A2B10G10R10, G16R16, V16U16, V8U8, Q8W8V8U8 map to a Vulkan format directly (the X formats with the alpha swizzled to one).
- A4R4G4B4 and X4R4G4B4 are converted to B8G8R8A8.
- The bump map formats L6V5U5 and X8L8V8U8 are converted to half floats (du, dv, l, 1).
- DXT1 maps to BC1, DXT2 and DXT3 to BC2, DXT4 and DXT5 to BC3.
- R8G8B8, R3G3B2, A4L4, P8, A8R3G3B2, A8P8, W11V11U10 and A2W10V10U10 exist only as surfaces in system memory (R8G8B8 also for the readback of render targets). `CheckDeviceFormat` reports them as not available for textures.

The back buffer is always a B8G8R8A8 image whatever format the game asks for. Depth formats are D16, D24X8 and D24S8. D16 is `D16_UNORM`. D24X8 is `X8_D24_UNORM_PACK32`, or `D32_SFLOAT` where the GPU lacks it. D24S8 is the format `VkCore.c` picked at device creation: `D24_UNORM_S8_UINT`, else `D32_SFLOAT_S8_UINT`.

A texture lock hands out system memory. The unlock or `UpdateTexture` converts and uploads it through the frame ring. Locking a render target or the back buffer submits the frame and waits for the GPU (`Vk_Flush`), then reads back. Vertex and index buffers are host visible. A `D3DLOCK_DISCARD` lock of a buffer that the GPU still reads swaps in a spare buffer (up to 7 per buffer, then it waits). `NOOVERWRITE` writes in place. Any other lock of a busy buffer waits.

## Vulkan core

`VkCore.c` loads `vulkan-1.dll` at run time and needs a 1.3 loader. It uses these features: dynamic rendering, synchronization2, the extended dynamic state of 1.3 (cull mode, front face, topology, depth and stencil state) and `VK_KHR_push_descriptor`. The device creation fails when dynamic rendering or synchronization2 is missing. Optional features are switched on when the GPU has them: robust buffer access, non solid fill, anisotropy, BC compression.

The GPU is picked by `SCRAP_VK_DEVICE`: an index, or `vendor:device` in hex. Without it the first discrete GPU with Vulkan 1.3 is used, else the first GPU. The queue family must draw and present. The swapchain format is B8G8R8A8 or R8G8B8A8 UNORM. The present mode is FIFO. Without vsync (presentation interval immediate) it is immediate or mailbox if available. A window with size zero has no swapchain until it has a size again.

There are two frames in flight (`VK_FRAMES`). Each has a command buffer, a fence, an acquire semaphore and a 16 MB ring buffer in host visible memory. The ring holds everything that changes per frame: vertices of the UP draws, the uniform blocks, uploads. `Vk_RingAlloc` hands out aligned pieces. When the ring is full, the recorded commands are submitted and the work goes on in the next frame. `Vk_Cmd` starts recording on demand and waits for the fence of its frame first. Each frame has a serial. Objects that are destroyed while the GPU may use them go to a defer list and are freed once their serial is done (`Vk_DeferBuffer`, `Vk_DeferImage`).

## A draw

The render pass is dynamic rendering on the render target and depth buffer of the device. It starts with the first clear or draw and ends before a transfer, a change of targets or the present.

`Vkd_Prepare` does everything up to the draw command:

1. The state of the fixed function goes into a uniform block (`VkdFF`, std140, the layout of `shaders/ff.glsl`) in the frame ring: the three matrices and four texture matrices, material, 8 lights, fog, texture factor, the stage states and the bump matrices, the pixel shader constants, and flags for the vertex format.
2. Textures are moved to the shader read layout outside the render pass. The block and the textures (four stages, a white texture for a stage without one) go in as push descriptors. The sampler comes from the texture stage states and is cached (64 at most).
3. The pipeline comes from a cache. The key is the vertex format and stride, the topology class, the blend state (enable, source and destination factor, blend op), the color write mask, the fill mode, the pixel shader and the depth format. The cache holds 512 pipelines.
4. Everything else is dynamic state: viewport, scissor, cull mode, front face, topology, depth test, write and compare op, depth bias, the stencil state with its masks and reference.

The viewport is set upside down (negative height) so the Direct3D clip space and the y down screen convention work unchanged. Vertex attributes that a format lacks come from a small buffer read with stride 0 (texture coordinate 0 0 0 1, zero normal, white diffuse, black specular). Triangle fans and strips use the Vulkan topologies. Indexed draws use 16 or 32 bit indices and the base vertex of `SetIndices`. UP draws copy their vertices (and indices) into the ring.

## Shaders

`shaders/ff.vert` and `ff.frag` are the fixed function as one uber shader driven by the uniform block: vertex lighting with up to 8 directional, point and spot lights, color sources, vertex and table fog, texture coordinate generation and transforms for four stages, the texture stage operations and arguments of Direct3D 8, bump environment mapping and the specular add. `frag.glsl` holds the end of every fragment shader: the alpha test and the fog, as Direct3D 8 does them after the stages or the pixel shader. The matrices are used as they are in memory, which makes them transposed in GLSL.

The pixel shaders of the game (`bmp/<name>.psh` in `Data.packed`) are hand ports to GLSL, one file `shaders/ps/<name>.frag` each, 30 in all. `ps.glsl` gives them the ps 1.1 vocabulary: `t0` to `t3` as `Tex(n)`, `TexBem`, the constants `c(i)`, `v0` and `v1`, `Bx2`, `Dp3`, and `Out` that clamps r0 and calls `Finish`.

`CMakeLists.txt` finds `glslangValidator`, compiles each shader to SPIR-V words (`--target-env vulkan1.3`) into `build/shaders/*.inc`, and writes `ps_all.inc`, a table of name and code for every `shaders/ps/*.frag`. `VkDraw.c` includes these, so there is no shader file at run time.

`Rd_DevCreatePixelShader` gets the file name of the shader, not its D3D8 tokens. `Vkd_CreatePixelShader` takes the base name without path and extension, in lower case, applies one alias (`fire` uses the `electric` port) and looks it up in the table. Each call returns a new handle (up to 128) that holds the index of the port. A name without a port logs `no GLSL port of the pixel shader` and returns 0. The handle of the game is not the index, the Direct3D 8 numbers differ (the tests mask that).

A pixel shader draw uses the same vertex shader and uniform block as the fixed function. The pipeline key has the shader, and the constants come from `SetPixelShaderConstant`.

## Draws that sample their own target

Direct3D 8 allows a draw to read the render target it writes when each pixel reads its own texel. `DuDv_Filter` does this on the bump target. Vulkan does not allow it. When a bound texture is the current render target, `Vkd_Feedback` ends the pass, copies the target into a second image (`vkd_feedback`, made again when the size or format changes) and the draw samples the copy.

## Present

The game draws into the back buffer image. `Vkd_Present` ends the pass, uploads the gamma ramp if `SetGammaRamp` changed it (a 256 x 1 texture with three 16 bit channels), acquires a swapchain image and draws a full screen triangle (`present.vert`, `present.frag`). The fragment shader samples the back buffer with a linear filter, so a window of another size than the back buffer is scaled, and maps each channel through the ramp. Then it submits and presents. If the swapchain is out of date it is made again at the size of the window.

## Videos

`Engine/Video.c` plays the `.vds` and `.avi` files of the game (AVI with XviD video and PCM sound) with FFmpeg. `tools/ffmpeg.sh` builds FFmpeg 7.1.1 as shared LGPL DLLs for mingw i686 with only the AVI demuxer, the MPEG-4 part 2 and PCM decoders and swscale. CMake links the import libraries and stops without them.

`Video_Step` is called once per pass of the message loop and never waits. It reads up to 32 packets, decodes the pictures that are due by the clock (started with the first picture) and draws the last due one. Video packets go to the decoder. Sound packets are decoded and written into a 1 MB ring. The sound mixer (`Snd_StreamPcm`, include/scrap/snd.h) plays that ring as a stream: its callback (on the mixer thread) copies from the ring and fills the rest with silence. The intro videos run before the sound system, so the video player starts the mixer itself. The ring uses atomic head and tail, so there is no lock. `sws_scale` converts the picture to BGRA straight into the locked X8R8G8B8 texture of the render device, with the mirror flags of `PlayVideoFX`. `Video_Draw` clears the back buffer and draws the texture with bars that keep the ratio, through the same `Rd_*` calls as the rest.

## Debug aids

- `SCRAP_VK_VALIDATION=1` turns on `VK_LAYER_KHRONOS_validation` at instance creation. `tools/test/vkcheck.sh` runs the game with it and prints the validation errors that are not from gamescope or Wine (`tools/test/vk-foreign-vuids.txt` lists those).
- `SCRAP_VK_TRACE=<seconds>` writes every draw of one frame into `scrapdecomp.log`: target size and format, vertex format, state values and the texture stages. The frame is the first one that starts that many seconds after the device was created. From a test script, `SDTest.Call('Vkd_TraceStart', 'v')` traces the frames up to the next present.
- `SCRAP_RD_DUMP=<dir>` and `SCRAP_RD_DUMP_EVERY=<n>` (default 100) write every nth presented frame as `vulkan<frame>.bmp`, read back from the back buffer.
- The scrap MCP server (`tools/mcp/`, see [its README](../tools/mcp/README.md)) runs the game headless, takes screenshots, shows the trace and runs the validation check.

## Not there or approximated

- The D3DX mesh calls (`Rd_XMeshDrawSubset`, attribute table, face and vertex counts) are stubs. They draw nothing and return zero.
- No depth textures. Depth formats exist only as depth surfaces.
- No multisampling. `CheckDeviceMultiSampleType` accepts only type 0, pipelines use one sample.
- Four texture stages are sampled, although the device keeps the state of eight.
- No palette formats and no 8 bit packed color formats as textures.
- Only one adapter and one swapchain.
- The caps are fixed values, not read from the GPU (except the texture size).
- The pixel shader tokens are not parsed, the shader is picked by its file name. A shader without a hand port is not created.
- Formats with a conversion cost a CPU pass on every upload.
