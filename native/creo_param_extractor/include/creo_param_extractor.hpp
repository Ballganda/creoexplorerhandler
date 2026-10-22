#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace creo {

struct ExplorerParams {
  std::optional<std::string> authors;
  std::optional<std::string> tags;
  std::optional<std::string> comments;
  std::optional<std::string> title;
  std::optional<std::string> subject;
};

// Upper bound on bytes read/scanned for USER-parameter extraction. Caps Explorer
// memory and CPU when folders contain very large .prt/.asm files. Parameter
// tables in typical Creo models sit well below this.
constexpr std::size_t kMaxParamScanBytes = 32ull * 1024ull * 1024ull;

// Returns true when path looks like a Creo part/assembly name Windows may
// register under numbered extensions (.1, .2, …).
bool is_creo_model_filename(std::string_view path_or_name);

// True when buffer starts with a Creo UGC header.
bool is_creo_ugc_file(const std::uint8_t* data, std::size_t size);

// Extract AUTHORS / TAGS / COMMENTS / TITLE / SUBJECT string parameters.
// Numeric (Real) parameters are skipped (left empty).
ExplorerParams extract_explorer_params(const std::uint8_t* data, std::size_t size);

ExplorerParams extract_explorer_params_from_file(const std::string& path);

// Extract one string USER parameter by exact Creo name (usually UPPERCASE).
// Real/Number values return empty.
std::optional<std::string> extract_string_param(const std::uint8_t* data,
                                                std::size_t size,
                                                std::string_view param_name);

// Single-pass extract for many Creo names. Keys in the result are exact name
// strings that were requested and found (string type only). Last e2 match wins
// per name (same semantics as extract_string_param). Scans at most
// kMaxParamScanBytes of the buffer.
std::unordered_map<std::string, std::string> extract_string_params(
    const std::uint8_t* data, std::size_t size,
    const std::vector<std::string>& param_names);

}  // namespace creo
