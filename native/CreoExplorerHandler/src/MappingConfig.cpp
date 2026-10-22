#include "MappingConfig.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

namespace mapping {
namespace {

std::string trim(std::string_view s) {
  std::size_t begin = 0;
  while (begin < s.size() &&
         std::isspace(static_cast<unsigned char>(s[begin]))) {
    ++begin;
  }
  std::size_t end = s.size();
  while (end > begin &&
         std::isspace(static_cast<unsigned char>(s[end - 1]))) {
    --end;
  }
  return std::string(s.substr(begin, end - begin));
}

std::string to_lower(std::string_view s) {
  std::string out(s);
  for (char& c : out) {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  return out;
}

bool parse_quoted(std::string_view line, std::size_t& i, std::string& out) {
  while (i < line.size() &&
         std::isspace(static_cast<unsigned char>(line[i]))) {
    ++i;
  }
  if (i >= line.size() || line[i] != '"') {
    return false;
  }
  ++i;
  const std::size_t start = i;
  while (i < line.size() && line[i] != '"') {
    ++i;
  }
  if (i >= line.size()) {
    return false;
  }
  out.assign(line.substr(start, i - start));
  ++i;
  return true;
}

struct Alias {
  const char* header;
  const char* canonical;
};

// Friendly headers and common mistaken System.* names → real canonical names.
// PSGetPropertyKeyFromName only accepts registered schema names; unknown
// System.Foo entries are skipped silently by the property store.
constexpr Alias kAliases[] = {
    {"Authors", "System.Author"},
    {"Author", "System.Author"},
    {"Tags", "System.Keywords"},
    {"Keywords", "System.Keywords"},
    {"Comments", "System.Comment"},
    {"Comment", "System.Comment"},
    {"Title", "System.Title"},
    {"Subject", "System.Subject"},
    {"Company", "System.Company"},
    {"Category", "System.Category"},
    {"Categories", "System.Category"},
    {"Status", "System.ContentStatus"},
    {"Content status", "System.ContentStatus"},
    {"Content Status", "System.ContentStatus"},
    {"Copyright", "System.Copyright"},
    {"Last author", "System.Document.LastAuthor"},
    {"Last Author", "System.Document.LastAuthor"},
    {"Revision number", "System.Document.RevisionNumber"},
    {"Revision Number", "System.Document.RevisionNumber"},
    {"Revision", "System.Document.RevisionNumber"},
    {"Version number", "System.Document.Version"},
    {"Version Number", "System.Document.Version"},
    {"Version", "System.Document.Version"},
    {"File version", "System.FileVersion"},
    {"File Version", "System.FileVersion"},
    {"Manager", "System.Document.Manager"},
    {"Client ID", "System.Document.ClientID"},
    {"Document ID", "System.Document.DocumentID"},
    {"Contributor", "System.Document.Contributor"},
    {"Language", "System.Language"},
    {"Source", "System.Source"},
    {"Project", "System.Project"},
    {"File description", "System.FileDescription"},
    {"File Description", "System.FileDescription"},
    {"Software used", "System.SoftwareUsed"},
    {"Software Used", "System.SoftwareUsed"},
    {"Application name", "System.ApplicationName"},
    {"Application Name", "System.ApplicationName"},
    {"Program name", "System.ApplicationName"},
    {"Program Name", "System.ApplicationName"},
    {"Content type", "System.ContentType"},
    {"Content Type", "System.ContentType"},
    {"Initials", "System.Contact.Initials"},
    {"Department", "System.Contact.Department"},
    // Explorer column "Location" (not "Office location").
    {"Location", "System.Calendar.Location"},
    {"Office location", "System.Contact.OfficeLocation"},
    {"Office Location", "System.Contact.OfficeLocation"},
    {"Subtitle", "System.Media.SubTitle"},
    {"SubTitle", "System.Media.SubTitle"},
    // Explorer column "Manufacturer" (not "Camera maker").
    {"Manufacturer", "System.Devices.Manufacturer"},
    {"Camera maker", "System.Photo.CameraManufacturer"},
    {"Camera Maker", "System.Photo.CameraManufacturer"},
    // Explorer column "Classification" (Devices group, next to Manufacturer).
    // MapColumnToSCID column 147 => System.Devices.PrimaryCategory
    // ({D08DD4C0-3A9E-462E-8290-7B636B2576B9}, 10). Not Devices.Category,
    // and System.Devices.Classification does not exist.
    {"Classification", "System.Devices.PrimaryCategory"},
    {"Sensitivity", "System.SensitivityText"},

    // Mistaken short / wrong-group System.* names.
    {"System.Initials", "System.Contact.Initials"},
    {"System.Department", "System.Contact.Department"},
    {"System.Location", "System.Calendar.Location"},
    {"System.Contact.OfficeLocation", "System.Contact.OfficeLocation"},
    {"System.Subtitle", "System.Media.SubTitle"},
    {"System.Media.Subtitle", "System.Media.SubTitle"},
    {"System.ProgramName", "System.ApplicationName"},
    {"System.Classification", "System.Devices.PrimaryCategory"},
    {"System.Devices.Classification", "System.Devices.PrimaryCategory"},
    {"System.Devices.Category", "System.Devices.PrimaryCategory"},
    {"System.SensitivityText", "System.SensitivityText"},
    {"System.Manufacturer", "System.Devices.Manufacturer"},
    {"System.Photo.CameraManufacturer", "System.Photo.CameraManufacturer"},
    // Explorer Details column "Product version" is System.Software.ProductVersion
    // (VERSIONINFO FMTID, PID 8) — not System.ProductVersion.
    {"System.ProductVersion", "System.Software.ProductVersion"},
    {"System.Software.ProductVersion", "System.Software.ProductVersion"},
    {"Product version", "System.Software.ProductVersion"},
    {"Product Version", "System.Software.ProductVersion"},
};

}  // namespace

Config default_config() {
  Config cfg;
  cfg.entries = {
      {"Authors", "AUTHORS"},
      {"Tags", "TAGS"},
      {"Comments", "COMMENTS"},
      {"Title", "TITLE"},
      {"Subject", "SUBJECT"},
  };
  return cfg;
}

Config parse_config_text(std::string_view text) {
  Config cfg;
  std::size_t pos = 0;
  while (pos <= text.size()) {
    const std::size_t eol = text.find('\n', pos);
    std::string_view line = text.substr(
        pos, eol == std::string_view::npos ? text.size() - pos : eol - pos);
    pos = (eol == std::string_view::npos) ? text.size() + 1 : eol + 1;

    if (!line.empty() && line.back() == '\r') {
      line.remove_suffix(1);
    }
    const std::string trimmed = trim(line);
    if (trimmed.empty()) {
      continue;
    }
    if (trimmed.size() >= 2 && trimmed[0] == '/' && trimmed[1] == '/') {
      continue;
    }

    std::size_t i = 0;
    std::string explorer;
    std::string creo;
    if (!parse_quoted(trimmed, i, explorer) ||
        !parse_quoted(trimmed, i, creo)) {
      continue;
    }
    while (i < trimmed.size() &&
           std::isspace(static_cast<unsigned char>(trimmed[i]))) {
      ++i;
    }
    if (i != trimmed.size() || explorer.empty() || creo.empty()) {
      continue;
    }
    cfg.entries.push_back(Entry{std::move(explorer), std::move(creo)});
  }
  return cfg;
}

std::optional<std::string> read_text_file(const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    return std::nullopt;
  }
  std::ostringstream oss;
  oss << in.rdbuf();
  if (!in && !in.eof()) {
    return std::nullopt;
  }
  return oss.str();
}

std::optional<std::string> resolve_canonical_name(
    std::string_view explorer_name) {
  const std::string trimmed = trim(explorer_name);
  if (trimmed.empty()) {
    return std::nullopt;
  }

  // Aliases first so mistaken System.Initials etc. are rewritten.
  const std::string needle = to_lower(trimmed);
  for (const Alias& alias : kAliases) {
    if (to_lower(alias.header) == needle) {
      return std::string(alias.canonical);
    }
  }

  if (trimmed.size() > 7 &&
      (trimmed.compare(0, 7, "System.") == 0 ||
       trimmed.compare(0, 7, "system.") == 0)) {
    std::string out = trimmed;
    out.replace(0, 7, "System.");
    return out;
  }

  return std::nullopt;
}

bool is_author_property(std::string_view canonical_name) {
  return to_lower(canonical_name) == "system.author";
}

bool is_multivalue_string_property(std::string_view canonical_name) {
  const std::string lower = to_lower(canonical_name);
  return lower == "system.devices.category" ||
         lower == "system.devices.categoryplural" ||
         lower == "system.devices.categorygroup";
}

std::wstring build_property_list(const Config& config) {
  std::vector<std::string> names;
  auto add = [&](const std::string& name) {
    for (const auto& existing : names) {
      if (to_lower(existing) == to_lower(name)) {
        return;
      }
    }
    names.push_back(name);
  };

  for (const Entry& entry : config.entries) {
    if (auto canon = resolve_canonical_name(entry.explorer_name)) {
      add(*canon);
    }
  }
  add("System.ItemTypeText");
  add("System.Size");
  add("System.DateModified");
  add("System.DateCreated");

  std::wstring out = L"prop:";
  for (std::size_t i = 0; i < names.size(); ++i) {
    if (i > 0) {
      out.push_back(L';');
    }
    out.append(names[i].begin(), names[i].end());
  }
  return out;
}

Config load_config_beside_module(std::string_view module_path) {
  std::string map_path;
  const auto slash = module_path.find_last_of("/\\");
  if (slash == std::string_view::npos) {
    map_path = "CreoExplorerHandler.map";
  } else {
    map_path.assign(module_path.substr(0, slash + 1));
    map_path += "CreoExplorerHandler.map";
  }

  if (auto text = read_text_file(map_path)) {
    Config cfg = parse_config_text(*text);
    if (!cfg.entries.empty()) {
      cfg.source_path = map_path;
      return cfg;
    }
  }
  return default_config();
}

}  // namespace mapping
