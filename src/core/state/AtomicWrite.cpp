#include "core/state/AtomicWrite.h"

#include <cstdio>
#include <fstream>
#include <iterator>
#include <system_error>

#ifdef _WIN32
#include <io.h> // _commit, _fileno
#else
#include <unistd.h> // fsync
#endif

namespace bei::core {

namespace {

/// Write and flush `content` to `file` down to the disk. Returns false on any error.
bool writeDurable(std::filesystem::path const& file, std::string_view content) {
#ifdef _WIN32
    std::FILE* f = nullptr;
    if (_wfopen_s(&f, file.c_str(), L"wb") != 0 || !f) return false;
#else
    std::FILE* f = std::fopen(file.c_str(), "wb");
    if (!f) return false;
#endif
    bool ok = std::fwrite(content.data(), 1, content.size(), f) == content.size();
    ok      = std::fflush(f) == 0 && ok;
#ifdef _WIN32
    ok = _commit(_fileno(f)) == 0 && ok;
#else
    ok = fsync(fileno(f)) == 0 && ok;
#endif
    ok = std::fclose(f) == 0 && ok;
    return ok;
}

} // namespace

bool writeFileAtomic(std::filesystem::path const& file, std::string_view content) {
    namespace fs = std::filesystem;
    if (file.empty()) return false;
    std::error_code ec;
    if (file.has_parent_path()) fs::create_directories(file.parent_path(), ec);

    fs::path tmp  = file;
    tmp          += ".tmp";
    if (!writeDurable(tmp, content)) {
        fs::remove(tmp, ec);
        return false;
    }
    // std::filesystem::rename replaces an existing target (MoveFileExW with REPLACE_EXISTING on Windows).
    // If it fails, the previous file is kept as is; the caller can retry on the next save.
    fs::rename(tmp, file, ec);
    if (ec) {
        std::error_code ignored;
        fs::remove(tmp, ignored);
        return false;
    }
    return true;
}

std::optional<std::string> readFile(std::filesystem::path const& file) {
    std::ifstream in(file, std::ios::binary);
    if (!in) return std::nullopt;
    std::string data{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
    if (in.bad()) return std::nullopt;
    return data;
}

} // namespace bei::core
