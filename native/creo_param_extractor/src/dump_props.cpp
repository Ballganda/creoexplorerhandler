#include "creo_param_extractor.hpp"

#include <iostream>
#include <optional>
#include <string>

static void print_field(const char* name, const std::optional<std::string>& value) {
  std::cout << name << "=";
  if (value) {
    std::cout << *value;
  }
  std::cout << "\n";
}

int main(int argc, char** argv) {
  if (argc < 2) {
    std::cerr << "Usage: dump_props <creo-file>\n";
    return 2;
  }
  const std::string path = argv[1];
  if (!creo::is_creo_model_filename(path)) {
    std::cerr << "Filename does not look like a Creo .prt/.asm model: " << path
              << "\n";
    return 1;
  }
  const auto params = creo::extract_explorer_params_from_file(path);
  print_field("Authors", params.authors);
  print_field("Tags", params.tags);
  print_field("Comments", params.comments);
  print_field("Title", params.title);
  print_field("Subject", params.subject);
  return 0;
}
