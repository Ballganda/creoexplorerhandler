#include "MappingConfig.h"

#include <iostream>
#include <string>

static int g_failures = 0;

#define EXPECT_TRUE(cond)                                                      \
  do {                                                                         \
    if (!(cond)) {                                                             \
      std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << " " << #cond      \
                << "\n";                                                       \
      ++g_failures;                                                            \
    }                                                                          \
  } while (0)

#define EXPECT_EQ(a, b)                                                        \
  do {                                                                         \
    if ((a) != (b)) {                                                          \
      std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << " expected equal\n"; \
      ++g_failures;                                                            \
    }                                                                          \
  } while (0)

int main() {
  const char* text = R"(
// header comment
//"Windows explorer detail header name" "CREO_PARAMETER_NAME"

"Authors" "AUTHORS"
"Tags" "TAGS"
"System.Company" "COMPANY"
"NotARealHeader" "X"
bogus line
"" "EMPTY"
"EmptyCreo" ""
)";

  const auto cfg = mapping::parse_config_text(text);
  EXPECT_EQ(cfg.entries.size(), 4u);
  EXPECT_EQ(cfg.entries[0].explorer_name, "Authors");
  EXPECT_EQ(cfg.entries[0].creo_param, "AUTHORS");
  EXPECT_EQ(cfg.entries[1].explorer_name, "Tags");
  EXPECT_EQ(cfg.entries[2].explorer_name, "System.Company");
  EXPECT_EQ(cfg.entries[2].creo_param, "COMPANY");
  EXPECT_EQ(cfg.entries[3].explorer_name, "NotARealHeader");

  auto authors = mapping::resolve_canonical_name("Authors");
  EXPECT_TRUE(authors && *authors == "System.Author");
  auto company = mapping::resolve_canonical_name("System.Company");
  EXPECT_TRUE(company && *company == "System.Company");
  auto bad = mapping::resolve_canonical_name("NotARealHeader");
  EXPECT_TRUE(!bad);
  EXPECT_TRUE(mapping::is_author_property("System.Author"));
  EXPECT_TRUE(!mapping::is_author_property("System.Title"));

  const auto defaults = mapping::default_config();
  EXPECT_EQ(defaults.entries.size(), 5u);

  const auto list = mapping::build_property_list(cfg);
  EXPECT_TRUE(list.find(L"System.Author") != std::wstring::npos);
  EXPECT_TRUE(list.find(L"System.Company") != std::wstring::npos);
  EXPECT_TRUE(list.find(L"System.Size") != std::wstring::npos);

  if (g_failures != 0) {
    std::cerr << g_failures << " failure(s)\n";
    return 1;
  }
  std::cout << "mapping config tests ok\n";
  return 0;
}
