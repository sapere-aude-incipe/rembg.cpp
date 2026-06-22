# rembg.cpp

Background removal in portable C++.

`rembg.cpp` is a C++ library and CLI for running rembg-style background
removal without a Python runtime. It currently implements the U-2-Net ONNX
inference path used by `danielgatis/rembg`, returns transparent RGBA cutouts by
default, and can also return grayscale masks for workflows that need them.

This is not an official rembg project. It is an independent native
implementation intended for desktop apps, batch tools, games, and pipelines
that want the rembg workflow from C++.

---

## Why rembg.cpp?

Running `rembg` normally means Python, Pillow, NumPy, SciPy/scikit-image for
some features, and ONNX Runtime bindings. **rembg.cpp** keeps the core
background-removal path native. No Python runtime, no PIL objects, no NumPy
arrays. Just compile and remove backgrounds.

- **C++ library API**: pass files, encoded image bytes, or raw RGB pixels.
- **rembg-style output**: get transparent RGBA PNG cutouts by default.
- **Mask mode**: request the grayscale foreground mask when your pipeline needs
  the mask rather than the cutout.
- **Session reuse**: load the ONNX model once and process many images.
- **CPU-first inference**: uses the ONNX Runtime C/C++ API.
- **Small model option**: `u2netp.onnx` is about 4.4 MB.
- **Single library surface**: `rembg.cpp` + `rembg.h`, C++17.
- **Portable image IO**: vendored `stb_image` and `stb_image_write`.

## Status

Current release:

- Supported model family: U-2-Net-shaped ONNX models; `u2netp.onnx` is tested.
- Supported input formats through stb: JPEG, PNG, BMP, TGA.
- Supported library input forms: file path, encoded bytes, raw RGB buffer.
- Supported outputs: RGBA cutout image, encoded RGBA PNG bytes, grayscale mask,
  encoded mask PNG bytes.
- CLI modes: single image, folder batch, cutout output, mask-only output.
- Build system: CMake.
- Tested locally on Windows x64 with ONNX Runtime 1.23.2.

Not implemented yet: alpha matting, morphology-based mask post-processing, HTTP
server mode, raw RGB24 stream mode, automatic model download/cache management,
and the full rembg model zoo.

The resizing path is intentionally simple and portable. It uses bilinear
resampling, while upstream Python rembg uses Pillow's Lanczos resampling. Output
should be similar in intent, but not bit-identical to Python rembg.

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

# Remove background from one image. Writes transparent RGBA PNG.
./build/examples/rembg_mask \
  --model models/u2netp.onnx \
  --image photo.jpg \
  --output photo.out.png

# Process a folder.
./build/examples/rembg_mask \
  --model models/u2netp.onnx \
  --input-dir images \
  --out-dir output

# Return only the mask.
./build/examples/rembg_mask \
  --model models/u2netp.onnx \
  --image photo.jpg \
  --output mask.png \
  --only-mask
```

On Windows with Visual Studio/NMake, build from a developer prompt:

```powershell
cmake -S . -B build-nmake -G "NMake Makefiles" -DONNXRUNTIME_ROOT=deps\onnxruntime-win-x64-1.23.2
cmake --build build-nmake --config Release
```

## Usage as a Library

### File input and RGBA output

```cpp
#include "rembg.h"

rembg::params p;
p.model_path = "models/u2netp.onnx";
p.threads = 4;

rembg::session session(p);
rembg::image_rgba_u8 output = session.remove_file("input.png");
rembg::save_png("output.png", output);
```

### Input and output as bytes

```cpp
#include "rembg.h"
#include <fstream>

std::vector<unsigned char> read_all(const std::string & path) {
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

auto input = read_all("input.png");
auto output_png = session.remove_bytes(input);

std::ofstream out("output.png", std::ios::binary);
out.write(reinterpret_cast<const char *>(output_png.data()), output_png.size());
```

See `examples/rembg_bytes.cpp` for a complete byte-input/byte-output example.

### Raw RGB input, similar to NumPy/OpenCV-style workflows

```cpp
rembg::image_u8 image;
image.width = width;
image.height = height;
image.channels = 3;
image.pixels = rgb_pixels;

rembg::image_rgba_u8 output = session.remove(image);
```

### Mask-only output

```cpp
rembg::mask_u8 mask = session.predict_file("input.png");
rembg::save_mask_png("mask.png", mask);

std::vector<unsigned char> mask_png = session.predict_bytes_png(input_png_bytes);
```

### Session reuse for batch processing

```cpp
rembg::session session(p);

for (const auto & path : rembg::list_image_files("images")) {
    auto cutout = session.remove_file(path);
    rembg::save_png(rembg::default_output_path(path, "output"), cutout);
}
```

## CLI

```bash
rembg_mask --model u2netp.onnx --image photo.jpg --output photo.out.png
rembg_mask --model u2netp.onnx --input-dir images --out-dir output
rembg_mask --model u2netp.onnx --image photo.jpg --output mask.png --only-mask
```

Options:

- `--threads N`: ONNX Runtime CPU thread count, default `4`.
- `--model-size N`: square model input size, default `320`.
- `--only-mask`: save the grayscale foreground mask instead of an RGBA cutout.
- `--invert`: invert the generated mask.
- `--putalpha`: preserve RGB pixels and place the mask in the alpha channel.
- `--bgcolor r,g,b[,a]`: composite the cutout over a background color.

## Model

The tested default model is `u2netp.onnx`, the compact U-2-Net model used by
rembg. It is downloaded by:

```powershell
powershell -ExecutionPolicy Bypass -File scripts/download_u2netp.ps1
```

The model is not committed to this repository. Check the U-2-Net license before
redistributing model files.

## Relationship to rembg

`danielgatis/rembg` is the upstream Python project. `rembg.cpp` mirrors the core
U-2-Net remove path in native C++:

1. Decode the input image to RGB.
2. Resize and normalize for the ONNX model.
3. Run ONNX inference.
4. Min/max normalize the predicted mask.
5. Return either a transparent RGBA cutout or the mask.

Python rembg supports more frontends and options today, including HTTP server
mode, raw RGB24 stream mode, alpha matting, post-processing, many model
sessions, and Docker images. The goal of `rembg.cpp` is to grow toward the
useful native subset of those features while staying easy to compile and embed.

## Roadmap

- Add u2net, u2net_human_seg, silueta, ISNet, BiRefNet, BRIA, and SAM sessions
  where their ONNX input/output shapes can be supported cleanly.
- Add alpha matting and morphology-based post-processing.
- Add raw RGB24 stream CLI mode for FFmpeg-style pipelines.
- Add optional OpenCV/image codec backend for TIFF, WebP, and more formats.
- Add benchmark numbers across Windows, Linux, and macOS.
- Add release ZIPs with prebuilt Windows binaries.

## Releases

The current release line intentionally does not bundle model weights. Use the
download scripts or provide your own ONNX model path.

## License

`rembg.cpp` is released under the MIT License. See `LICENSE`.

Third-party license texts are kept in `licenses/` and summarized in
`THIRD_PARTY_NOTICES.md`.
