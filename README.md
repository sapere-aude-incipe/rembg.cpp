# rembg.cpp

Background-removal masks in portable C++.

`rembg.cpp` is a small C++ library and CLI for running U-2-Net style foreground
mask inference without Python. It currently supports the `u2netp.onnx` model
used by `danielgatis/rembg`, executes through ONNX Runtime, and writes grayscale
PNG masks.

This is not an official rembg project. It is an independent C++ implementation
of the image-only mask path for people who want a simple native executable or a
library they can embed in desktop tools.

---

## Why rembg.cpp?

Running `rembg` normally means Python, Pillow, NumPy, and ONNX Runtime bindings.
`rembg.cpp` keeps the runtime path native:

- **No Python at inference time**: compile a C++ library and run masks directly.
- **Small model option**: `u2netp.onnx` is about 4.4 MB.
- **CPU-first**: uses ONNX Runtime CPU execution by default.
- **Simple API**: load a model once, run many images, save PNG masks.
- **Batch CLI**: process one image or a folder of images.
- **Portable image IO**: uses vendored `stb_image` and `stb_image_write`.

## Status

First public version:

- Supported model: `u2netp.onnx`
- Supported input formats: JPEG, PNG, BMP, TGA
- Output: 8-bit grayscale PNG mask
- Build system: CMake
- Tested locally on Windows x64 with ONNX Runtime 1.23.2

The resizing path is intentionally simple and portable. It uses bilinear
resampling, while upstream Python rembg uses Pillow's Lanczos resampling. Masks
should be very similar in intent, but not bit-identical to Python rembg.

## Quick Start

```bash
git clone https://github.com/sapere-aude-incipe/rembg.cpp
cd rembg.cpp

# Download ONNX Runtime and u2netp.onnx.
# Windows PowerShell:
powershell -ExecutionPolicy Bypass -File scripts/download_onnxruntime.ps1
powershell -ExecutionPolicy Bypass -File scripts/download_u2netp.ps1

# Build.
cmake -S . -B build -DONNXRUNTIME_ROOT=deps/onnxruntime-win-x64-1.23.2
cmake --build build --config Release

# Segment one image.
./build/examples/rembg_mask \
  --model models/u2netp.onnx \
  --image photo.jpg \
  --output mask.png

# Segment a folder.
./build/examples/rembg_mask \
  --model models/u2netp.onnx \
  --input-dir images \
  --out-dir masks
```

On Windows with Visual Studio/NMake, build from a developer prompt:

```powershell
cmake -S . -B build-nmake -G "NMake Makefiles" -DONNXRUNTIME_ROOT=deps\onnxruntime-win-x64-1.23.2
cmake --build build-nmake --config Release
```

## C++ API

```cpp
#include "rembg.h"

rembg::params p;
p.model_path = "models/u2netp.onnx";
p.threads = 4;

rembg::session session(p);
rembg::mask_u8 mask = session.predict_file("photo.jpg");
rembg::save_mask_png("mask.png", mask);
```

For repeated work, create one `rembg::session` and reuse it for many images.

```cpp
rembg::session session(p);
for (const auto & path : rembg::list_image_files("images")) {
    auto mask = session.predict_file(path);
    rembg::save_mask_png(rembg::default_output_path(path, "masks"), mask);
}
```

## CLI

```bash
rembg_mask --model u2netp.onnx --image photo.jpg --output mask.png
rembg_mask --model u2netp.onnx --image photo.jpg --out-dir masks
rembg_mask --model u2netp.onnx --input-dir images --out-dir masks
```

Options:

- `--threads N`: ONNX Runtime CPU thread count, default `4`.
- `--model-size N`: square model input size, default `320`.
- `--invert`: save foreground black on white background.

## Model

The default supported model is `u2netp.onnx`, the compact U-2-Net model used by
rembg. It is downloaded by:

```powershell
powershell -ExecutionPolicy Bypass -File scripts/download_u2netp.ps1
```

The model is not committed to this repository. Check the U-2-Net license before
redistributing model files.

## Dependencies

- ONNX Runtime C/C++ prebuilt package
- stb image headers, vendored under `third_party/stb`
- U-2-Net compatible ONNX model file, downloaded separately

No Python dependency is required at runtime.

## Relationship to rembg

`danielgatis/rembg` is the upstream Python project that popularized this simple
background-removal workflow. `rembg.cpp` mirrors the key `u2netp` inference
steps:

1. Convert image to RGB and resize to `320x320`.
2. Normalize with ImageNet mean and standard deviation.
3. Run ONNX inference.
4. Min/max normalize the first predicted mask.
5. Resize the mask back to the original image size and save PNG.

The implementation is new C++ code and is not a drop-in replacement for all
features in Python rembg.

## Roadmap

- Add u2net full-size model support.
- Add alpha-matte RGBA output helper.
- Add optional OpenCV/image codec backend for TIFF and more formats.
- Add benchmark numbers across Windows, Linux, and macOS.
- Add release ZIPs with prebuilt Windows binaries.

## Releases

The intended first release is `v0.1.0`, containing the library, CLI, CMake
build, Windows CPU validation, and u2netp model download helper. Release assets
should not bundle model weights unless their license obligations are reviewed
for that specific distribution.

## License

`rembg.cpp` is released under the MIT License. See `LICENSE`.

Third-party license texts are kept in `licenses/` and summarized in
`THIRD_PARTY_NOTICES.md`.
