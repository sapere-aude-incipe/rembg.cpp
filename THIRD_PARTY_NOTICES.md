# Third-Party Notices

This project is MIT licensed, but it interoperates with and references several
third-party projects. Keep this file and the `licenses/` directory with source
or binary redistributions.

## rembg

- Project: danielgatis/rembg
- Use: reference behavior for U-2-Net preprocessing, ONNX inference, and mask
  postprocessing.
- License: MIT License.
- Notice: the upstream license is included at `licenses/rembg-LICENSE.txt`.

## ONNX Runtime

- Project: Microsoft ONNX Runtime.
- Use: CPU inference runtime for `.onnx` model execution.
- License: MIT License.
- Notice: license and third-party notices are included at
  `licenses/onnxruntime-LICENSE.txt` and
  `licenses/onnxruntime-ThirdPartyNotices.txt`.

## U-2-Net / u2netp

- Project: U-2-Net.
- Use: foreground/background segmentation model. The default example uses
  `u2netp.onnx`, downloaded separately from rembg's model release assets.
- License: Apache License 2.0.
- Notice: the upstream license is included at `licenses/U-2-Net-LICENSE.txt`.

## stb

- Project: stb single-file image libraries.
- Use: image loading and PNG mask writing.
- License: public domain or MIT License, at the user's option.
- Notice: the vendored headers under `third_party/stb/` contain the upstream
  license text.
