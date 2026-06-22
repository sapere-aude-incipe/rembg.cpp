# Changelog

## v0.2.0 - 2026-06-22

- Broadened the library from mask generation to rembg-style background removal.
- Added RGBA cutout output through `session::remove` and `session::remove_file`.
- Added encoded image byte input and encoded PNG byte output helpers.
- Added `image_rgba_u8`, `remove_options`, background color compositing, and
  `put_alpha` mode.
- Added `examples/rembg_bytes.cpp` for encoded byte input/output.
- Added CLI `--only-mask`, `--putalpha`, and `--bgcolor` options.
- Changed CLI default output from mask PNG to transparent RGBA cutout PNG.
- Kept mask output available for pipelines with `--only-mask`.

## v0.1.0 - 2026-06-22

- Added `rembg` static library with public `rembg.h` API.
- Added `rembg_mask` CLI example for single-image and folder mask generation.
- Added U-2-Net `u2netp.onnx` preprocessing/postprocessing path.
- Added CMake build with ONNX Runtime integration.
- Added Windows helper scripts for ONNX Runtime and `u2netp.onnx` downloads.
- Added third-party notices and upstream license copies.
