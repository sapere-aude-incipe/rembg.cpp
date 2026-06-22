#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#endif

#include "rembg.h"

#include <onnxruntime_cxx_api.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cctype>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#endif

namespace rembg {
namespace {

bool is_sep(char c) {
    return c == '/' || c == '\\';
}

std::string path_join(const std::string & a, const std::string & b) {
    if (a.empty()) return b;
    if (b.empty()) return a;
    if (is_sep(a.back())) return a + b;
#ifdef _WIN32
    return a + "\\" + b;
#else
    return a + "/" + b;
#endif
}

std::string basename_of(const std::string & path) {
    const size_t pos = path.find_last_of("/\\");
    return pos == std::string::npos ? path : path.substr(pos + 1);
}

std::string stem_of(const std::string & path) {
    const std::string base = basename_of(path);
    const size_t dot = base.find_last_of('.');
    if (dot == std::string::npos || dot == 0) return base;
    return base.substr(0, dot);
}

std::string lower_copy(std::string s) {
    for (char & c : s) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return s;
}

template <typename T>
T bilerp(T c00, T c10, T c01, T c11, float tx, float ty) {
    const float a = static_cast<float>(c00) * (1.0f - tx) + static_cast<float>(c10) * tx;
    const float b = static_cast<float>(c01) * (1.0f - tx) + static_cast<float>(c11) * tx;
    const float v = a * (1.0f - ty) + b * ty;
    return static_cast<T>(std::min(255.0f, std::max(0.0f, v + 0.5f)));
}

std::vector<unsigned char> resize_u8(
    const unsigned char * src,
    int src_w,
    int src_h,
    int channels,
    int dst_w,
    int dst_h) {
    std::vector<unsigned char> dst(static_cast<size_t>(dst_w) * dst_h * channels);
    if (src_w == dst_w && src_h == dst_h) {
        std::copy(src, src + static_cast<size_t>(src_w) * src_h * channels, dst.begin());
        return dst;
    }

    const float scale_x = static_cast<float>(src_w) / dst_w;
    const float scale_y = static_cast<float>(src_h) / dst_h;

    for (int y = 0; y < dst_h; ++y) {
        const float sy = (y + 0.5f) * scale_y - 0.5f;
        const int y0 = std::max(0, std::min(src_h - 1, static_cast<int>(std::floor(sy))));
        const int y1 = std::max(0, std::min(src_h - 1, y0 + 1));
        const float ty = sy - std::floor(sy);
        for (int x = 0; x < dst_w; ++x) {
            const float sx = (x + 0.5f) * scale_x - 0.5f;
            const int x0 = std::max(0, std::min(src_w - 1, static_cast<int>(std::floor(sx))));
            const int x1 = std::max(0, std::min(src_w - 1, x0 + 1));
            const float tx = sx - std::floor(sx);
            for (int c = 0; c < channels; ++c) {
                const size_t i00 = (static_cast<size_t>(y0) * src_w + x0) * channels + c;
                const size_t i10 = (static_cast<size_t>(y0) * src_w + x1) * channels + c;
                const size_t i01 = (static_cast<size_t>(y1) * src_w + x0) * channels + c;
                const size_t i11 = (static_cast<size_t>(y1) * src_w + x1) * channels + c;
                dst[(static_cast<size_t>(y) * dst_w + x) * channels + c] =
                    bilerp(src[i00], src[i10], src[i01], src[i11], tx, ty);
            }
        }
    }
    return dst;
}

std::vector<float> preprocess(const image_u8 & img, int side) {
    if (img.width <= 0 || img.height <= 0 || img.channels != 3 || img.pixels.empty()) {
        throw std::invalid_argument("invalid RGB image");
    }

    std::vector<unsigned char> resized = resize_u8(
        img.pixels.data(),
        img.width,
        img.height,
        3,
        side,
        side);

    float max_value = 0.0f;
    for (unsigned char v : resized) {
        max_value = std::max(max_value, static_cast<float>(v));
    }
    if (max_value < 1e-6f) max_value = 1e-6f;

    const float mean[3] = {0.485f, 0.456f, 0.406f};
    const float stdv[3] = {0.229f, 0.224f, 0.225f};
    std::vector<float> input(static_cast<size_t>(3) * side * side);

    for (int y = 0; y < side; ++y) {
        for (int x = 0; x < side; ++x) {
            const size_t src = (static_cast<size_t>(y) * side + x) * 3;
            for (int ch = 0; ch < 3; ++ch) {
                const float value = resized[src + ch] / max_value;
                input[static_cast<size_t>(ch) * side * side + static_cast<size_t>(y) * side + x] =
                    (value - mean[ch]) / stdv[ch];
            }
        }
    }

    return input;
}

std::vector<unsigned char> normalize_mask(const float * pred, size_t count, bool invert) {
    float mn = std::numeric_limits<float>::infinity();
    float mx = -std::numeric_limits<float>::infinity();
    for (size_t i = 0; i < count; ++i) {
        mn = std::min(mn, pred[i]);
        mx = std::max(mx, pred[i]);
    }

    const float denom = std::max(mx - mn, 1e-6f);
    std::vector<unsigned char> mask(count);
    for (size_t i = 0; i < count; ++i) {
        float value = (pred[i] - mn) / denom;
        value = std::min(1.0f, std::max(0.0f, value));
        const unsigned char out = static_cast<unsigned char>(value * 255.0f + 0.5f);
        mask[i] = invert ? static_cast<unsigned char>(255 - out) : out;
    }
    return mask;
}

void png_write_callback(void * context, void * data, int size) {
    auto * out = static_cast<std::vector<unsigned char> *>(context);
    const auto * bytes = static_cast<unsigned char *>(data);
    out->insert(out->end(), bytes, bytes + size);
}

#ifdef _WIN32
std::wstring widen(const std::string & s) {
    if (s.empty()) return std::wstring();
    const int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    if (len <= 0) {
        return std::wstring(s.begin(), s.end());
    }
    std::wstring out(static_cast<size_t>(len - 1), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, out.data(), len);
    return out;
}
#endif

} // namespace

struct session::impl {
    explicit impl(const params & p)
        : settings(p),
          env(ORT_LOGGING_LEVEL_WARNING, "rembg.cpp") {
        if (settings.model_path.empty()) {
            throw std::invalid_argument("model_path is required");
        }
        if (settings.model_size < 1) {
            throw std::invalid_argument("model_size must be positive");
        }
        if (settings.threads < 1) {
            throw std::invalid_argument("threads must be positive");
        }

        Ort::SessionOptions session_options;
        session_options.SetIntraOpNumThreads(settings.threads);
        session_options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

#ifdef _WIN32
        const std::wstring model_path = widen(settings.model_path);
        ort_session = std::make_unique<Ort::Session>(env, model_path.c_str(), session_options);
#else
        ort_session = std::make_unique<Ort::Session>(env, settings.model_path.c_str(), session_options);
#endif

        Ort::AllocatorWithDefaultOptions allocator;
        auto input = ort_session->GetInputNameAllocated(0, allocator);
        auto output = ort_session->GetOutputNameAllocated(0, allocator);
        input_name = input.get();
        output_name = output.get();
    }

    mask_u8 predict(const image_u8 & image) const {
        std::vector<float> input = preprocess(image, settings.model_size);
        std::array<int64_t, 4> input_shape = {1, 3, settings.model_size, settings.model_size};
        Ort::MemoryInfo memory_info = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
        Ort::Value input_tensor = Ort::Value::CreateTensor<float>(
            memory_info,
            input.data(),
            input.size(),
            input_shape.data(),
            input_shape.size());

        const char * input_names[] = {input_name.c_str()};
        const char * output_names[] = {output_name.c_str()};
        auto outputs = ort_session->Run(
            Ort::RunOptions{nullptr},
            input_names,
            &input_tensor,
            1,
            output_names,
            1);

        float * out = outputs[0].GetTensorMutableData<float>();
        const auto shape = outputs[0].GetTensorTypeAndShapeInfo().GetShape();
        if (shape.size() < 4 || shape[shape.size() - 1] <= 0 || shape[shape.size() - 2] <= 0) {
            throw std::runtime_error("unexpected ONNX output shape");
        }

        const int mask_h = static_cast<int>(shape[shape.size() - 2]);
        const int mask_w = static_cast<int>(shape[shape.size() - 1]);
        if (mask_w != mask_h) {
            throw std::runtime_error("unexpected non-square ONNX output shape");
        }

        const size_t mask_count = static_cast<size_t>(mask_w) * mask_h;
        std::vector<unsigned char> normalized = normalize_mask(out, mask_count, settings.invert);
        mask_u8 result;
        result.width = image.width;
        result.height = image.height;
        result.pixels = resize_u8(normalized.data(), mask_w, mask_h, 1, image.width, image.height);
        return result;
    }

    params settings;
    Ort::Env env;
    std::unique_ptr<Ort::Session> ort_session;
    std::string input_name;
    std::string output_name;
};

session::session(const params & p)
    : impl_(std::make_unique<impl>(p)) {
}

session::~session() = default;
session::session(session &&) noexcept = default;
session & session::operator=(session &&) noexcept = default;

mask_u8 session::predict(const image_u8 & image) const {
    return impl_->predict(image);
}

mask_u8 session::predict_file(const std::string & image_path) const {
    return predict(load_image_file(image_path));
}

std::vector<unsigned char> session::predict_bytes_png(const unsigned char * data, size_t size) const {
    return encode_png(predict(load_image_bytes(data, size)));
}

std::vector<unsigned char> session::predict_bytes_png(const std::vector<unsigned char> & data) const {
    return predict_bytes_png(data.data(), data.size());
}

image_rgba_u8 session::remove(const image_u8 & image, const remove_options & options) const {
    return apply_alpha(image, predict(image), options);
}

image_rgba_u8 session::remove_file(const std::string & image_path, const remove_options & options) const {
    return remove(load_image_file(image_path), options);
}

std::vector<unsigned char> session::remove_bytes(
    const unsigned char * data,
    size_t size,
    const remove_options & options) const {
    return encode_png(remove(load_image_bytes(data, size), options));
}

std::vector<unsigned char> session::remove_bytes(
    const std::vector<unsigned char> & data,
    const remove_options & options) const {
    return remove_bytes(data.data(), data.size(), options);
}

image_u8 load_image_file(const std::string & path) {
    int w = 0, h = 0, c = 0;
    unsigned char * data = stbi_load(path.c_str(), &w, &h, &c, 3);
    if (!data || w <= 0 || h <= 0) {
        if (data) stbi_image_free(data);
        throw std::runtime_error("failed to load image: " + path);
    }

    image_u8 image;
    image.width = w;
    image.height = h;
    image.channels = 3;
    image.pixels.assign(data, data + static_cast<size_t>(w) * h * 3);
    stbi_image_free(data);
    return image;
}

image_u8 load_image_bytes(const unsigned char * data, size_t size) {
    if (data == nullptr || size == 0 || size > static_cast<size_t>(std::numeric_limits<int>::max())) {
        throw std::invalid_argument("invalid encoded image bytes");
    }

    int w = 0, h = 0, c = 0;
    unsigned char * decoded = stbi_load_from_memory(
        data,
        static_cast<int>(size),
        &w,
        &h,
        &c,
        3);
    if (!decoded || w <= 0 || h <= 0) {
        if (decoded) stbi_image_free(decoded);
        throw std::runtime_error("failed to decode image bytes");
    }

    image_u8 image;
    image.width = w;
    image.height = h;
    image.channels = 3;
    image.pixels.assign(decoded, decoded + static_cast<size_t>(w) * h * 3);
    stbi_image_free(decoded);
    return image;
}

image_u8 load_image_bytes(const std::vector<unsigned char> & data) {
    return load_image_bytes(data.data(), data.size());
}

image_rgba_u8 apply_alpha(const image_u8 & image, const mask_u8 & mask, const remove_options & options) {
    if (image.width <= 0 || image.height <= 0 || image.channels != 3 || image.pixels.empty()) {
        throw std::invalid_argument("invalid RGB image");
    }
    if (mask.width != image.width || mask.height != image.height || mask.pixels.empty()) {
        throw std::invalid_argument("mask size does not match image");
    }

    image_rgba_u8 out;
    out.width = image.width;
    out.height = image.height;
    out.channels = 4;
    out.pixels.resize(static_cast<size_t>(image.width) * image.height * 4);

    for (int y = 0; y < image.height; ++y) {
        for (int x = 0; x < image.width; ++x) {
            const size_t src = (static_cast<size_t>(y) * image.width + x) * 3;
            const size_t dst = (static_cast<size_t>(y) * image.width + x) * 4;
            const unsigned char alpha = mask.pixels[static_cast<size_t>(y) * image.width + x];
            const float a = alpha / 255.0f;

            unsigned char r = image.pixels[src + 0];
            unsigned char g = image.pixels[src + 1];
            unsigned char b = image.pixels[src + 2];
            unsigned char out_alpha = alpha;

            if (options.mode == cutout_mode::composite) {
                r = static_cast<unsigned char>(r * a + 0.5f);
                g = static_cast<unsigned char>(g * a + 0.5f);
                b = static_cast<unsigned char>(b * a + 0.5f);
            }

            if (options.apply_background) {
                const float ba = options.background.a / 255.0f;
                const float out_a = a + ba * (1.0f - a);
                if (out_a > 1e-6f) {
                    r = static_cast<unsigned char>(((image.pixels[src + 0] * a) + (options.background.r * ba * (1.0f - a))) / out_a + 0.5f);
                    g = static_cast<unsigned char>(((image.pixels[src + 1] * a) + (options.background.g * ba * (1.0f - a))) / out_a + 0.5f);
                    b = static_cast<unsigned char>(((image.pixels[src + 2] * a) + (options.background.b * ba * (1.0f - a))) / out_a + 0.5f);
                    out_alpha = static_cast<unsigned char>(out_a * 255.0f + 0.5f);
                } else {
                    r = g = b = out_alpha = 0;
                }
            }

            out.pixels[dst + 0] = r;
            out.pixels[dst + 1] = g;
            out.pixels[dst + 2] = b;
            out.pixels[dst + 3] = out_alpha;
        }
    }

    return out;
}

void save_mask_png(const std::string & path, const mask_u8 & mask) {
    if (mask.width <= 0 || mask.height <= 0 || mask.pixels.empty()) {
        throw std::invalid_argument("invalid mask");
    }
    if (stbi_write_png(path.c_str(), mask.width, mask.height, 1, mask.pixels.data(), mask.width) == 0) {
        throw std::runtime_error("failed to save mask: " + path);
    }
}

void save_png(const std::string & path, const image_rgba_u8 & image) {
    if (image.width <= 0 || image.height <= 0 || image.channels != 4 || image.pixels.empty()) {
        throw std::invalid_argument("invalid RGBA image");
    }
    if (stbi_write_png(path.c_str(), image.width, image.height, 4, image.pixels.data(), image.width * 4) == 0) {
        throw std::runtime_error("failed to save PNG: " + path);
    }
}

std::vector<unsigned char> encode_png(const mask_u8 & mask) {
    if (mask.width <= 0 || mask.height <= 0 || mask.pixels.empty()) {
        throw std::invalid_argument("invalid mask");
    }
    std::vector<unsigned char> out;
    if (stbi_write_png_to_func(
            png_write_callback,
            &out,
            mask.width,
            mask.height,
            1,
            mask.pixels.data(),
            mask.width) == 0) {
        throw std::runtime_error("failed to encode mask PNG");
    }
    return out;
}

std::vector<unsigned char> encode_png(const image_rgba_u8 & image) {
    if (image.width <= 0 || image.height <= 0 || image.channels != 4 || image.pixels.empty()) {
        throw std::invalid_argument("invalid RGBA image");
    }
    std::vector<unsigned char> out;
    if (stbi_write_png_to_func(
            png_write_callback,
            &out,
            image.width,
            image.height,
            4,
            image.pixels.data(),
            image.width * 4) == 0) {
        throw std::runtime_error("failed to encode PNG");
    }
    return out;
}

bool has_image_extension(const std::string & path) {
    const std::string base = lower_copy(basename_of(path));
    const size_t dot = base.find_last_of('.');
    if (dot == std::string::npos) return false;
    const std::string ext = base.substr(dot);
    return ext == ".jpg" || ext == ".jpeg" || ext == ".png" ||
           ext == ".bmp" || ext == ".tga";
}

std::vector<std::string> list_image_files(const std::string & dir) {
    std::vector<std::string> out;
#ifdef _WIN32
    WIN32_FIND_DATAA ffd;
    HANDLE h = FindFirstFileA(path_join(dir, "*").c_str(), &ffd);
    if (h == INVALID_HANDLE_VALUE) {
        throw std::runtime_error("failed to list input directory: " + dir);
    }
    do {
        if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        const std::string name = ffd.cFileName;
        if (has_image_extension(name)) out.push_back(path_join(dir, name));
    } while (FindNextFileA(h, &ffd));
    FindClose(h);
#else
    DIR * d = opendir(dir.c_str());
    if (!d) {
        throw std::runtime_error("failed to list input directory: " + dir);
    }
    while (dirent * e = readdir(d)) {
        if (e->d_type == DT_DIR) continue;
        const std::string name = e->d_name;
        if (has_image_extension(name)) out.push_back(path_join(dir, name));
    }
    closedir(d);
#endif
    std::sort(out.begin(), out.end());
    return out;
}

std::string sanitize_filename(const std::string & value) {
    std::string out;
    out.reserve(value.size());
    for (unsigned char c : value) {
        if (std::isalnum(c) || c == '-' || c == '_') {
            out.push_back(static_cast<char>(c));
        } else if (!out.empty() && out.back() != '_') {
            out.push_back('_');
        }
    }
    while (!out.empty() && out.back() == '_') {
        out.pop_back();
    }
    return out.empty() ? "mask" : out;
}

std::string default_output_path(const std::string & image_path, const std::string & out_dir, bool only_mask) {
    return path_join(out_dir, sanitize_filename(stem_of(image_path)) + (only_mask ? "__rembg__mask.png" : "__rembg.png"));
}

bool ensure_directory(const std::string & path) {
    if (path.empty()) return true;
#ifdef _WIN32
    const DWORD attr = GetFileAttributesA(path.c_str());
    if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY)) return true;
    return _mkdir(path.c_str()) == 0 || errno == EEXIST;
#else
    struct stat st {};
    if (stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) return true;
    return mkdir(path.c_str(), 0755) == 0 || errno == EEXIST;
#endif
}

} // namespace rembg
