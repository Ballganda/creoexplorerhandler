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
  // NotARealHeader is skipped; Authors/Tags/Company resolve.
  EXPECT_EQ(cfg.resolved.size(), 3u);
  EXPECT_EQ(cfg.resolved[0].canonical_name, "System.Author");
  EXPECT_EQ(cfg.resolved[0].creo_param, "AUTHORS");
  EXPECT_EQ(cfg.resolved[2].canonical_name, "System.Company");

  auto authors = mapping::resolve_canonical_name("Authors");
  EXPECT_TRUE(authors && *authors == "System.Author");
  auto company = mapping::resolve_canonical_name("System.Company");
  EXPECT_TRUE(company && *company == "System.Company");
  auto initials = mapping::resolve_canonical_name("System.Initials");
  EXPECT_TRUE(initials && *initials == "System.Contact.Initials");
  auto subtitle = mapping::resolve_canonical_name("System.Subtitle");
  EXPECT_TRUE(subtitle && *subtitle == "System.Media.SubTitle");
  auto program = mapping::resolve_canonical_name("System.ProgramName");
  EXPECT_TRUE(program && *program == "System.ApplicationName");
  auto product = mapping::resolve_canonical_name("System.ProductVersion");
  EXPECT_TRUE(product && *product == "System.Software.ProductVersion");
  auto product_col = mapping::resolve_canonical_name("Product version");
  EXPECT_TRUE(product_col && *product_col == "System.Software.ProductVersion");
  auto loc = mapping::resolve_canonical_name("Location");
  EXPECT_TRUE(loc && *loc == "System.Calendar.Location");
  auto mfg = mapping::resolve_canonical_name("Manufacturer");
  EXPECT_TRUE(mfg && *mfg == "System.Devices.Manufacturer");
  auto cls = mapping::resolve_canonical_name("Classification");
  EXPECT_TRUE(cls && *cls == "System.Devices.PrimaryCategory");
  auto cls_wrong = mapping::resolve_canonical_name("System.Devices.Classification");
  EXPECT_TRUE(cls_wrong && *cls_wrong == "System.Devices.PrimaryCategory");
  auto cls_short = mapping::resolve_canonical_name("System.Classification");
  EXPECT_TRUE(cls_short && *cls_short == "System.Devices.PrimaryCategory");
  auto cls_cat = mapping::resolve_canonical_name("System.Devices.Category");
  EXPECT_TRUE(cls_cat && *cls_cat == "System.Devices.PrimaryCategory");
  auto wrong_loc = mapping::resolve_canonical_name("System.Contact.OfficeLocation");
  EXPECT_TRUE(wrong_loc && *wrong_loc == "System.Contact.OfficeLocation");
  auto bad = mapping::resolve_canonical_name("NotARealHeader");
  EXPECT_TRUE(!bad);
  EXPECT_TRUE(mapping::is_author_property("System.Author"));
  EXPECT_TRUE(!mapping::is_author_property("System.Title"));
  EXPECT_TRUE(mapping::is_multivalue_string_property("System.Devices.Category"));
  EXPECT_TRUE(!mapping::is_multivalue_string_property("System.Devices.Manufacturer"));

  const auto defaults = mapping::default_config();
  EXPECT_EQ(defaults.entries.size(), 5u);
  EXPECT_EQ(defaults.resolved.size(), 5u);

  mapping::clear_config_cache_for_tests();
  const auto& cached1 = mapping::cached_config_beside_module(
      "/workspace/native/CreoExplorerHandler/CreoExplorerHandler.dll");
  EXPECT_TRUE(!cached1.resolved.empty());
  const auto& cached2 = mapping::cached_config_beside_module(
      "/workspace/native/CreoExplorerHandler/CreoExplorerHandler.dll");
  EXPECT_TRUE(&cached1 == &cached2);

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
