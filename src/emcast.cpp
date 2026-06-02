#include "emcast/emcast.hpp"

#include <fstream>

namespace emcast {

Bytes read_file(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) throw Error("could not open file: " + path);
    return Bytes((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

void write_file(const std::string& path, const Bytes& data) {
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f) throw Error("could not open file for writing: " + path);
    if (!data.empty())
        f.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    if (!f) throw Error("error writing file: " + path);
}

}  // namespace emcast
