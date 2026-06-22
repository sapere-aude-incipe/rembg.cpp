#pragma once

#include <memory>
#include <string>
#include <vector>

namespace rembg {

struct image_u8 {
    int width = 0;
    int height = 0;
    int channels = 3;
    std::vector<unsigned char> pixels;
};

struct mask_u8 {
    int width = 0;
    int height = 0;
    std::vector<unsigned char> pixels;
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

private:
    struct impl;
    std::unique_ptr<impl> impl_;
};

image_u8 load_image_file(const std::string & path);
void save_mask_png(const std::string & path, const mask_u8 & mask);

bool has_image_extension(const std::string & path);
std::vector<std::string> list_image_files(const std::string & dir);

std::string sanitize_filename(const std::string & value);
std::string default_output_path(const std::string & image_path, const std::string & out_dir);
bool ensure_directory(const std::string & path);

} // namespace rembg
