#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace bei::core {

/// Write `content` to `file` atomically: write `<file>.tmp` next to it, flush, then rename over
/// the target. Creates parent directories. On failure the previous file is left intact and the
/// temp file is removed. Returns true on success.
bool writeFileAtomic(std::filesystem::path const& file, std::string_view content);

/// Read a whole file. nullopt if it can't be opened.
[[nodiscard]] std::optional<std::string> readFile(std::filesystem::path const& file);

} // namespace bei::core
