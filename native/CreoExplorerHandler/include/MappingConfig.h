#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace mapping {

struct Entry {
  // Explorer header or canonical name as written in the map file
  // (e.g. Authors or System.Author).
  std::string explorer_name;
  // Creo USER parameter name (usually UPPERCASE).
  std::string creo_param;
};

struct Config {
  std::vector<Entry> entries;
  // Path that was loaded, or empty when built-in defaults are used.
  std::string source_path;
};

// Default AUTHORS/TAGS/COMMENTS/TITLE/SUBJECT mappings.
Config default_config();

// Parse map file text. Lines starting with // are comments. Mapping lines:
//   "Windows explorer detail header name" "CREO_PARAMETER_NAME"
// Unknown/malformed mapping lines are skipped.
Config parse_config_text(std::string_view text);

// Load UTF-8 text from path. Returns nullopt if the file cannot be read.
std::optional<std::string> read_text_file(const std::string& path);

// Resolve a map token to a Windows canonical property name (System.*).
// Accepts System.* as-is, or common Explorer header aliases (Authors → System.Author).
std::optional<std::string> resolve_canonical_name(std::string_view explorer_name);

// Build prop:System.A;System.B;... list for FullDetails / PreviewDetails / InfoTip.
// Always appends Size / DateModified / DateCreated / ItemTypeText.
std::wstring build_property_list(const Config& config);

// True when the canonical property should be stored as a string vector (Authors).
bool is_author_property(std::string_view canonical_name);

// True when the property schema type is Multivalue String (e.g. Devices.Category).
bool is_multivalue_string_property(std::string_view canonical_name);

// Load CreoExplorerHandler.map from the folder that contains module_path.
// Falls back to default_config() when the file is missing or has no mappings.
Config load_config_beside_module(std::string_view module_path);

}  // namespace mapping
