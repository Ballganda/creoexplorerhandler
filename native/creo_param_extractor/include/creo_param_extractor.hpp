#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace creo {

struct ExplorerParams {
  std::optional<std::string> authors;
  std::optional<std::string> tags;
  std::optional<std::string> comments;
  std::optional<std::string> title;
  std::optional<std::string> subject;
};

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

}  // namespace creo
