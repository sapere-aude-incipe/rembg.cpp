#include "rembg.h"

#include <cstdio>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

struct options {
    rembg::params params;
    std::string image_path;
    std::string input_dir;
    std::string output_path;
    std::string out_dir = "masks";
};

void print_usage(const char * exe) {
    std::fprintf(stderr,
        "Usage:\n"
        "  %s --model MODEL.onnx --image IMAGE --out-dir DIR [options]\n"
        "  %s --model MODEL.onnx --image IMAGE --output MASK.png [options]\n"
        "  %s --model MODEL.onnx --input-dir DIR --out-dir DIR [options]\n"
        "\n"
        "Options:\n"
        "  --output PATH  Exact output path for single-image mode\n"
        "  --model-size N  U2Net input side length (default: 320)\n"
        "  --threads N     ONNX Runtime CPU thread count (default: 4)\n"
        "  --invert        Save foreground black on white background\n",
        exe, exe, exe);
}

bool parse_int(const char * s, int & out) {
    char * end = nullptr;
    const long value = std::strtol(s, &end, 10);
    if (!s[0] || (end && *end) || value < 1 || value > std::numeric_limits<int>::max()) {
        return false;
    }
    out = static_cast<int>(value);
    return true;
}

bool require_value(int argc, char ** argv, int & i, std::string & out) {
    if (i + 1 >= argc) {
        std::fprintf(stderr, "Missing value for %s\n", argv[i]);
        return false;
    }
    out = argv[++i];
    return true;
}

std::string dirname_of(const std::string & path) {
    const size_t pos = path.find_last_of("/\\");
    return pos == std::string::npos ? std::string() : path.substr(0, pos);
}

bool parse_args(int argc, char ** argv, options & opt) {
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--model" && require_value(argc, argv, i, opt.params.model_path)) {
        } else if (arg == "--image" && require_value(argc, argv, i, opt.image_path)) {
        } else if (arg == "--input-dir" && require_value(argc, argv, i, opt.input_dir)) {
        } else if (arg == "--output" && require_value(argc, argv, i, opt.output_path)) {
        } else if (arg == "--out-dir" && require_value(argc, argv, i, opt.out_dir)) {
        } else if (arg == "--model-size") {
            if (i + 1 >= argc || !parse_int(argv[++i], opt.params.model_size)) {
                std::fprintf(stderr, "Invalid --model-size\n");
                return false;
            }
        } else if (arg == "--threads") {
            if (i + 1 >= argc || !parse_int(argv[++i], opt.params.threads)) {
                std::fprintf(stderr, "Invalid --threads\n");
                return false;
            }
        } else if (arg == "--invert") {
            opt.params.invert = true;
        } else if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            std::exit(0);
        } else {
            std::fprintf(stderr, "Unknown argument: %s\n", arg.c_str());
            return false;
        }
    }

    if (opt.params.model_path.empty()) {
        std::fprintf(stderr, "Missing --model\n");
        return false;
    }
    if (opt.image_path.empty() == opt.input_dir.empty()) {
        std::fprintf(stderr, "Specify exactly one of --image or --input-dir\n");
        return false;
    }
    if (!opt.output_path.empty() && opt.image_path.empty()) {
        std::fprintf(stderr, "--output is only valid with --image\n");
        return false;
    }
    return true;
}

std::string output_path_for(const std::string & image_path, const options & opt) {
    if (!opt.output_path.empty()) {
        return opt.output_path;
    }
    return rembg::default_output_path(image_path, opt.out_dir);
}

} // namespace

int main(int argc, char ** argv) {
    options opt;
    if (!parse_args(argc, argv, opt)) {
        print_usage(argv[0]);
        return 2;
    }

    try {
        if (opt.output_path.empty()) {
            if (!rembg::ensure_directory(opt.out_dir)) {
                std::fprintf(stderr, "Failed to create output directory: %s\n", opt.out_dir.c_str());
                return 2;
            }
        } else {
            const std::string output_dir = dirname_of(opt.output_path);
            if (!output_dir.empty() && !rembg::ensure_directory(output_dir)) {
                std::fprintf(stderr, "Failed to create output directory: %s\n", output_dir.c_str());
                return 2;
            }
        }

        std::vector<std::string> images;
        if (!opt.image_path.empty()) {
            images.push_back(opt.image_path);
        } else {
            images = rembg::list_image_files(opt.input_dir);
        }

        std::printf("Loading model: %s\n", opt.params.model_path.c_str());
        rembg::session session(opt.params);

        int ok = 0;
        for (const std::string & image : images) {
            const std::string output_path = output_path_for(image, opt);
            std::printf("Image: %s\n", image.c_str());
            rembg::mask_u8 mask = session.predict_file(image);
            rembg::save_mask_png(output_path, mask);
            std::printf("  Saved mask: %s\n", output_path.c_str());
            ++ok;
        }

        std::printf(
            "Done: %d/%zu images processed. Output: %s\n",
            ok,
            images.size(),
            opt.output_path.empty() ? opt.out_dir.c_str() : opt.output_path.c_str());
    } catch (const std::exception & e) {
        std::fprintf(stderr, "Error: %s\n", e.what());
        return 1;
    }

    return 0;
}
