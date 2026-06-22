#include "rembg.h"

#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

namespace {

std::vector<unsigned char> read_all(const std::string & path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw std::runtime_error("failed to open input: " + path);
    }
    return {
        std::istreambuf_iterator<char>(in),
        std::istreambuf_iterator<char>()
    };
}

void write_all(const std::string & path, const std::vector<unsigned char> & bytes) {
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        throw std::runtime_error("failed to open output: " + path);
    }
    out.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
}

} // namespace

int main(int argc, char ** argv) {
    if (argc != 4) {
        std::cerr << "Usage: " << argv[0] << " MODEL.onnx INPUT_IMAGE OUTPUT_PNG\n";
        return 2;
    }

    try {
        rembg::params params;
        params.model_path = argv[1];

        rembg::session session(params);
        const std::vector<unsigned char> input = read_all(argv[2]);
        const std::vector<unsigned char> output = session.remove_bytes(input);
        write_all(argv[3], output);
        std::cout << "Saved output: " << argv[3] << "\n";
    } catch (const std::exception & e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
