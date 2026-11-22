# Ally X Performance11

In the Performance10 Ocarina of Time log, one GPU-written command-list read waited 8.69 seconds during an 8.73-second gap between frames. The Vulkan worker had already received that tick. The log could not show when the graphics driver accepted the submission.

Performance11 lets the asynchronous Vulkan command parser put the waiting channel aside and run another ready channel. The first channel still needs its copy to finish. This can help when independent work is ready to run. A delay shared by all channels can still stall the frame.

Run `Play-Performance11.cmd` to enable `EDEN_NX2_LIST_YIELD=1`. The launcher also enables the wait trace. It can show whether the Vulkan queue submission had already returned successfully when the channel suspended. This flag describes queue submission, while the renderer separately checks actual GPU completion before reading command bytes.

The design follows [Lichii #752](https://github.com/zortos293/Lichii/pull/752). The Vulkan completion poll uses the 20-microsecond interval proposed in [Lichii #933](https://github.com/zortos293/Lichii/pull/933) after a Windows tester observed excessive polling. Performance11 keeps the original blocking path for synchronous emulation and for copies still awaiting submission.

Performance11 builds on the Performance10 pipeline-cache changes. It uses the same native Windows motion implementation as MotionFix2.

## Optional low-memory launcher

`Play-Performance11-LowMemory.cmd` enables compressed storage for eligible textures with uneven mip levels on integrated Vulkan GPUs. It uses the BC recompression setting you already chose. Performance10 logged a 1365 × 1365 texture with 90 layers and 11 mip levels in Ocarina of Time. It stored that texture as RGBA8 even though BC3 was selected. This launcher targets that memory cost directly. Some Windows AMD drivers may show fine grid lines on distant terrain with these textures. Use `Play-Performance11.cmd` for the original terrain handling.

For the observed 90-layer array, the image data alone takes about 852.5 MiB as RGBA8 or 214.4 MiB as BC3. Actual driver allocations can differ. For textures converted to BC, the Vulkan renderer on an integrated GPU can decode large arrays in smaller batches. For this array, decoding 23 layers at a time needs about 217.9 MiB of temporary RGBA8 data. A single layer larger than the target budget still needs enough space for that layer.

The optional format choice follows [Lichii #948](https://github.com/zortos293/Lichii/pull/948). The layer batching and staging lifetime changes follow [#1083](https://github.com/zortos293/Lichii/pull/1083), [#1796](https://github.com/zortos293/Lichii/pull/1796), and [#1823](https://github.com/zortos293/Lichii/pull/1823). Their published results and this game's logs do not measure Performance11 frame times on the Ally.
