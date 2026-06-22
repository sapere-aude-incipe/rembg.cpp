#pragma once

#include <memory>
#include <cstddef>
#include <string>
#include <vector>

namespace rembg {

struct image_u8 {
    int width = 0;
    int height = 0;
    int channels = 3;
    std::vector<unsigned char> pixels;
};

struct image_rgba_u8 {
    int width = 0;
    int height = 0;
    int channels = 4;
    std::vector<unsigned char> pixels;
};

struct mask_u8 {
    int width = 0;
    int height = 0;
    std::vector<unsigned char> pixels;
};

struct color_rgba {
    unsigned char r = 255;
    unsigned char g = 255;
    unsigned char b = 255;
    unsigned char a = 255;
};

enum class cutout_mode {
    composite,
    put_alpha
};

struct remove_options {
    cutout_mode mode = cutout_mode::composite;
    bool apply_background = false;
    color_rgba background;
};

struct params {
    std::string model_path;
    int model_size = 320;
    int threads = 4;
    bool invert = false;
};

class session {
public:
    explicit session(const params & p);
    ~session();

    session(session &&) noexcept;
    session & operator=(session &&) noexcept;

    session(const session &) = delete;
    session & operator=(const session &) = delete;

    mask_u8 predict(const image_u8 & image) const;
    mask_u8 predict_file(const std::string & image_path) const;
    std::vector<unsigned char> predict_bytes_png(const unsigned char * data, size_t size) const;
    std::vector<unsigned char> predict_bytes_png(const std::vector<unsigned char> & data) const;

    image_rgba_u8 remove(const image_u8 & image, const remove_options & options = {}) const;
    image_rgba_u8 remove_file(const std::string & image_path, const remove_options & options = {}) const;
    std::vector<unsigned char> remove_bytes(const unsigned char * data, size_t size, const remove_options & options = {}) const;
    std::vector<unsigned char> remove_bytes(const std::vector<unsigned char> & data, const remove_options & options = {}) const;

private:
    struct impl;
    std::unique_ptr<impl> impl_;
};

image_u8 load_image_file(const std::string & path);
image_u8 load_image_bytes(const unsigned char * data, size_t size);
image_u8 load_image_bytes(const std::vector<unsigned char> & data);

image_rgba_u8 apply_alpha(const image_u8 & image, const mask_u8 & mask, const remove_options & options = {});

void save_mask_png(const std::string & path, const mask_u8 & mask);
void save_png(const std::string & path, const image_rgba_u8 & image);

std::vector<unsigned char> encode_png(const mask_u8 & mask);
std::vector<unsigned char> encode_png(const image_rgba_u8 & image);

bool has_image_extension(const std::string & path);
std::vector<std::string> list_image_files(const std::string & dir);

std::string sanitize_filename(const std::string & value);
std::string default_output_path(const std::string & image_path, const std::string & out_dir, bool only_mask = false);
bool ensure_directory(const std::string & path);

} // namespace rembg
