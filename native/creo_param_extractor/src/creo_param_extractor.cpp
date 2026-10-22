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

struct NeedleState {
  std::string_view name;
  std::optional<std::string> best_e2;
  std::optional<std::string> best_e3;
};

bool name_boundary_ok(const std::uint8_t* data, std::size_t i) {
  if (i == 0) {
    return true;
  }
  const auto prev = data[i - 1];
  return !(is_printable_ascii(prev) &&
           (std::isalnum(prev) || prev == '_' || prev == '-'));
}

void apply_match(NeedleState& needle, const std::uint8_t* data, std::size_t size,
                 std::size_t name_end) {
  if (auto e2 = value_from_e2_table(data, size, name_end)) {
    needle.best_e2 = std::move(e2);
    return;
  }
  if (auto e3 = value_from_e3_near_name(data, size, name_end)) {
    needle.best_e3 = std::move(e3);
  }
}

std::size_t scan_limit(std::size_t size) {
  return std::min(size, kMaxParamScanBytes);
}

std::optional<std::string> find_param_string(const std::uint8_t* data,
                                            std::size_t size,
                                            std::string_view name) {
  if (name.empty() || size < name.size() + 2) {
    return std::nullopt;
  }

  NeedleState needle{name, std::nullopt, std::nullopt};
  const auto* needle_bytes = reinterpret_cast<const std::uint8_t*>(name.data());
  const std::size_t needle_len = name.size();
  const std::size_t limit = scan_limit(size);

  // Anchor on NAME\0 terminators instead of probing every byte offset.
  for (std::size_t end = needle_len; end < limit; ++end) {
    if (data[end] != 0x00) {
      continue;
    }
    const std::size_t start = end - needle_len;
    if (!std::equal(needle_bytes, needle_bytes + needle_len, data + start)) {
      continue;
    }
    if (!name_boundary_ok(data, start)) {
      continue;
    }
    apply_match(needle, data, size, end + 1);
  }

  if (needle.best_e2) {
    return needle.best_e2;
  }
  return needle.best_e3;
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

std::unordered_map<std::string, std::string> extract_string_params(
    const std::uint8_t* data, std::size_t size,
    const std::vector<std::string>& param_names) {
  std::unordered_map<std::string, std::string> out;
  if (!is_creo_ugc_file(data, size) || param_names.empty()) {
    return out;
  }

  std::vector<NeedleState> needles;
  needles.reserve(param_names.size());
  for (const std::string& name : param_names) {
    if (name.empty()) {
      continue;
    }
    bool dup = false;
    for (const auto& existing : needles) {
      if (existing.name == name) {
        dup = true;
        break;
      }
    }
    if (!dup) {
      needles.push_back(NeedleState{name, std::nullopt, std::nullopt});
    }
  }
  if (needles.empty()) {
    return out;
  }

  std::size_t min_len = needles.front().name.size();
  for (const auto& n : needles) {
    min_len = std::min(min_len, n.name.size());
  }

  // Small open-address table: FNV-1a of name bytes -> needle. Lookup from the
  // file buffer avoids per-NUL memcmp against every mapped parameter.
  constexpr std::size_t kBuckets = 64;
  NeedleState* table[kBuckets] = {};
  auto fnv1a = [](const std::uint8_t* p, std::size_t n) -> std::uint32_t {
    std::uint32_t h = 2166136261u;
    for (std::size_t i = 0; i < n; ++i) {
      h ^= p[i];
      h *= 16777619u;
    }
    return h;
  };
  auto insert = [&](NeedleState* needle) {
    const auto* p =
        reinterpret_cast<const std::uint8_t*>(needle->name.data());
    std::uint32_t h = fnv1a(p, needle->name.size());
    for (std::size_t i = 0; i < kBuckets; ++i) {
      NeedleState*& slot = table[(h + i) % kBuckets];
      if (slot == nullptr) {
        slot = needle;
        return;
      }
    }
  };
  for (NeedleState& needle : needles) {
    insert(&needle);
  }

  const std::size_t limit = scan_limit(size);
  // One pass anchored on \0: last e2/e3 match wins per name.
  for (std::size_t end = min_len; end < limit; ++end) {
    if (data[end] != 0x00) {
      continue;
    }
    // Candidate lengths: walk backward over a plausible identifier run.
    std::size_t start = end;
    while (start > 0) {
      const auto c = data[start - 1];
      if (!(std::isalnum(c) || c == '_' || c == '-')) {
        break;
      }
      --start;
      if (end - start > 64) {
        start = end - 64;
        break;
      }
    }
    if (start == end || !name_boundary_ok(data, start)) {
      continue;
    }
    const std::size_t len = end - start;
    const std::uint32_t h = fnv1a(data + start, len);
    for (std::size_t i = 0; i < kBuckets; ++i) {
      NeedleState* slot = table[(h + i) % kBuckets];
      if (slot == nullptr) {
        break;
      }
      if (slot->name.size() != len) {
        continue;
      }
      const auto* nb =
          reinterpret_cast<const std::uint8_t*>(slot->name.data());
      if (!std::equal(nb, nb + len, data + start)) {
        continue;
      }
      apply_match(*slot, data, size, end + 1);
      break;
    }
  }

  for (const NeedleState& needle : needles) {
    if (needle.best_e2) {
      out.emplace(std::string(needle.name), *needle.best_e2);
    } else if (needle.best_e3) {
      out.emplace(std::string(needle.name), *needle.best_e3);
    }
  }
  return out;
}

ExplorerParams extract_explorer_params(const std::uint8_t* data,
                                       std::size_t size) {
  ExplorerParams out;
  if (!is_creo_ugc_file(data, size)) {
    return out;
  }
  const std::vector<std::string> names = {"AUTHORS", "TAGS", "COMMENTS",
                                          "TITLE", "SUBJECT"};
  const auto values = extract_string_params(data, size, names);
  auto get = [&](const char* key) -> std::optional<std::string> {
    const auto it = values.find(key);
    if (it == values.end()) {
      return std::nullopt;
    }
    return it->second;
  };
  out.authors = get("AUTHORS");
  out.tags = get("TAGS");
  out.comments = get("COMMENTS");
  out.title = get("TITLE");
  out.subject = get("SUBJECT");
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
  if (len == 0) {
    return empty;
  }
  const std::size_t to_read = std::min(len, kMaxParamScanBytes);
  std::vector<std::uint8_t> buffer(to_read);
  in.read(reinterpret_cast<char*>(buffer.data()),
          static_cast<std::streamsize>(to_read));
  if (!in && !in.eof()) {
    return empty;
  }
  return extract_explorer_params(buffer.data(), buffer.size());
}

}  // namespace creo
