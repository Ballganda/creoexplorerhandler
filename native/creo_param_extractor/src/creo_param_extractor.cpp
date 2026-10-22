#include "creo_param_extractor.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <vector>

namespace creo {
namespace {

bool starts_with_ci(std::string_view text, std::string_view prefix) {
  if (text.size() < prefix.size()) {
    return false;
  }
  for (std::size_t i = 0; i < prefix.size(); ++i) {
    const auto a = static_cast<unsigned char>(text[i]);
    const auto b = static_cast<unsigned char>(prefix[i]);
    if (std::tolower(a) != std::tolower(b)) {
      return false;
    }
  }
  return true;
}

bool equal_ci(std::string_view a, std::string_view b) {
  if (a.size() != b.size()) {
    return false;
  }
  for (std::size_t i = 0; i < a.size(); ++i) {
    if (std::tolower(static_cast<unsigned char>(a[i])) !=
        std::tolower(static_cast<unsigned char>(b[i]))) {
      return false;
    }
  }
  return true;
}

bool contains_ci(std::string_view text, std::string_view needle) {
  if (needle.empty() || text.size() < needle.size()) {
    return false;
  }
  for (std::size_t i = 0; i + needle.size() <= text.size(); ++i) {
    if (equal_ci(text.substr(i, needle.size()), needle)) {
      return true;
    }
  }
  return false;
}

std::string_view filename_only(std::string_view path) {
  const auto slash = path.find_last_of("/\\");
  if (slash == std::string_view::npos) {
    return path;
  }
  return path.substr(slash + 1);
}

bool is_printable_ascii(unsigned char c) {
  return c >= 0x20 && c < 0x7f;
}

void trim_inplace(std::string& text) {
  while (!text.empty() &&
         std::isspace(static_cast<unsigned char>(text.front()))) {
    text.erase(text.begin());
  }
  while (!text.empty() &&
         std::isspace(static_cast<unsigned char>(text.back()))) {
    text.pop_back();
  }
}

// Creo Parameters type dropdown: 1=Integer?, 2=Real, 3=String (text).
// In the file these appear as ASCII '2' (0x32) and '3' (0x33).
constexpr std::uint8_t kTypeReal = 0x32;    // '2'
constexpr std::uint8_t kTypeString = 0x33;  // '3'

// After type '3', Creo sometimes inserts f7 <byte> before the value bytes.
std::optional<std::string> read_string_value(const std::uint8_t* data,
                                            std::size_t size,
                                            std::size_t pos) {
  if (pos >= size) {
    return std::nullopt;
  }
  if (data[pos] == 0xf7) {
    if (pos + 2 > size) {
      return std::nullopt;
    }
    pos += 2;
  }
  if (pos >= size || !is_printable_ascii(data[pos])) {
    return std::nullopt;
  }
  std::size_t end = pos;
  while (end < size && data[end] != 0x00) {
    if (!is_printable_ascii(data[end])) {
      return std::nullopt;
    }
    ++end;
    if (end - pos > 4096) {
      return std::nullopt;
    }
  }
  if (end >= size) {
    return std::nullopt;
  }
  std::string out(reinterpret_cast<const char*>(data + pos), end - pos);
  trim_inplace(out);
  if (out.empty()) {
    return std::nullopt;
  }
  return out;
}

// Primary binding used by the parameter value table:
//   NAME \0 ... \xe2 <type> [f7 xx] <value> \0
// The \xe2 form carries the live parameter value (e.g. BRO). A later \xe3 block
// may also store a title-cased label (Authors) — ignore that when \xe2 exists.
std::optional<std::string> value_from_e2_table(const std::uint8_t* data,
                                              std::size_t size,
                                              std::size_t name_end) {
  constexpr std::size_t kWindow = 24;
  const std::size_t limit = std::min(size, name_end + kWindow);
  for (std::size_t i = name_end; i + 1 < limit; ++i) {
    if (data[i] != 0xe2) {
      continue;
    }
    const auto type_byte = data[i + 1];
    if (type_byte == kTypeReal) {
      return std::nullopt;
    }
    if (type_byte == kTypeString) {
      return read_string_value(data, size, i + 2);
    }
  }
  return std::nullopt;
}

// Fallback for older/alternate encodings that use \xe3 <type> near the name.
std::optional<std::string> value_from_e3_near_name(const std::uint8_t* data,
                                                  std::size_t size,
                                                  std::size_t name_end) {
  constexpr std::size_t kWindow = 24;
  const std::size_t limit = std::min(size, name_end + kWindow);
  for (std::size_t i = name_end; i + 1 < limit; ++i) {
    if (data[i] != 0xe3) {
      continue;
    }
    const auto type_byte = data[i + 1];
    if (type_byte == kTypeReal) {
      return std::nullopt;
    }
    if (type_byte == kTypeString) {
      return read_string_value(data, size, i + 2);
    }
  }
  return std::nullopt;
}

std::optional<std::string> find_param_string(const std::uint8_t* data,
                                            std::size_t size,
                                            std::string_view name) {
  if (name.empty() || size < name.size() + 2) {
    return std::nullopt;
  }

  std::optional<std::string> best_e2;
  std::optional<std::string> best_e3;
  const auto* needle = reinterpret_cast<const std::uint8_t*>(name.data());
  const std::size_t needle_len = name.size();

  for (std::size_t i = 0; i + needle_len + 1 < size; ++i) {
    if (data[i + needle_len] != 0x00) {
      continue;
    }
    if (!std::equal(needle, needle + needle_len, data + i)) {
      continue;
    }
    if (i > 0) {
      const auto prev = data[i - 1];
      if (is_printable_ascii(prev) &&
          (std::isalnum(prev) || prev == '_' || prev == '-')) {
        continue;
      }
    }

    const std::size_t name_end = i + needle_len + 1;
    if (auto e2 = value_from_e2_table(data, size, name_end)) {
      best_e2 = std::move(e2);
      continue;
    }
    if (auto e3 = value_from_e3_near_name(data, size, name_end)) {
      best_e3 = std::move(e3);
    }
  }

  if (best_e2) {
    return best_e2;
  }
  return best_e3;
}

}  // namespace

bool is_creo_model_filename(std::string_view path_or_name) {
  const auto name = filename_only(path_or_name);
  if (name.empty()) {
    return false;
  }
  if (contains_ci(name, ".prt.") || contains_ci(name, ".asm.")) {
    return true;
  }
  if (name.size() >= 4 &&
      (starts_with_ci(name.substr(name.size() - 4), ".prt") ||
       starts_with_ci(name.substr(name.size() - 4), ".asm"))) {
    return true;
  }
  return false;
}

bool is_creo_ugc_file(const std::uint8_t* data, std::size_t size) {
  constexpr char kMagic[] = "#UGC:";
  constexpr std::size_t kMagicLen = sizeof(kMagic) - 1;
  if (data == nullptr || size < kMagicLen) {
    return false;
  }
  return std::equal(kMagic, kMagic + kMagicLen, data);
}

std::optional<std::string> extract_string_param(const std::uint8_t* data,
                                                std::size_t size,
                                                std::string_view param_name) {
  if (!is_creo_ugc_file(data, size) || param_name.empty()) {
    return std::nullopt;
  }
  return find_param_string(data, size, param_name);
}

ExplorerParams extract_explorer_params(const std::uint8_t* data,
                                       std::size_t size) {
  ExplorerParams out;
  if (!is_creo_ugc_file(data, size)) {
    return out;
  }
  out.authors = extract_string_param(data, size, "AUTHORS");
  out.tags = extract_string_param(data, size, "TAGS");
  out.comments = extract_string_param(data, size, "COMMENTS");
  out.title = extract_string_param(data, size, "TITLE");
  out.subject = extract_string_param(data, size, "SUBJECT");
  return out;
}

ExplorerParams extract_explorer_params_from_file(const std::string& path) {
  ExplorerParams empty;
  if (!is_creo_model_filename(path)) {
    return empty;
  }
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    return empty;
  }
  in.seekg(0, std::ios::end);
  const auto len = static_cast<std::size_t>(in.tellg());
  in.seekg(0, std::ios::beg);
  std::vector<std::uint8_t> buffer(len);
  if (len == 0) {
    return empty;
  }
  in.read(reinterpret_cast<char*>(buffer.data()),
          static_cast<std::streamsize>(len));
  if (!in) {
    return empty;
  }
  return extract_explorer_params(buffer.data(), buffer.size());
}

}  // namespace creo
