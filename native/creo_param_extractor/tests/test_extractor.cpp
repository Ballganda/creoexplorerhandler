#include "creo_param_extractor.hpp"

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

namespace fs = std::filesystem;

static int g_failures = 0;

#define EXPECT_TRUE(cond)                                                      \
  do {                                                                         \
    if (!(cond)) {                                                             \
      std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << " " << #cond      \
                << "\n";                                                       \
      ++g_failures;                                                            \
    }                                                                          \
  } while (0)

#define EXPECT_EQ_STR(opt, expected)                                           \
  do {                                                                         \
    if (!(opt) || *(opt) != (expected)) {                                      \
      std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << " expected \""     \
                << (expected) << "\" got "                                     \
                << ((opt) ? ("\"" + *(opt) + "\"") : std::string("<empty>"))   \
                << "\n";                                                       \
      ++g_failures;                                                            \
    }                                                                          \
  } while (0)

#define EXPECT_EMPTY(opt)                                                      \
  do {                                                                         \
    if (opt) {                                                                 \
      std::cerr << "FAIL " << __FILE__ << ":" << __LINE__                      \
                << " expected empty, got \"" << *(opt) << "\"\n";               \
      ++g_failures;                                                            \
    }                                                                          \
  } while (0)

static fs::path resolve_testdata(const char* relative) {
  const fs::path candidates[] = {
      fs::path("testdata") / relative,
      fs::path("../testdata") / relative,
      fs::path("../../testdata") / relative,
      fs::path("/workspace/testdata") / relative,
  };
  for (const auto& path : candidates) {
    if (fs::exists(path)) {
      return path;
    }
  }
  return candidates[0];
}

int main() {
  EXPECT_TRUE(creo::is_creo_model_filename("detailstest.prt.2"));
  EXPECT_TRUE(creo::is_creo_model_filename("C:\\models\\bracket.prt.12"));
  EXPECT_TRUE(creo::is_creo_model_filename("assy.asm.3"));
  EXPECT_TRUE(creo::is_creo_model_filename("part.prt"));
  EXPECT_TRUE(!creo::is_creo_model_filename("notes.txt.2"));
  EXPECT_TRUE(!creo::is_creo_model_filename("readme.2"));

  // Older sample: values were title-cased names; SUBJECT was Real.
  const auto old_sample = resolve_testdata("detailstest.prt.2");
  std::cout << "Old sample: " << old_sample << "\n";
  EXPECT_TRUE(fs::exists(old_sample));
  const auto old_params =
      creo::extract_explorer_params_from_file(old_sample.string());
  EXPECT_EQ_STR(old_params.authors, "Authors");
  EXPECT_EQ_STR(old_params.tags, "Tags");
  EXPECT_EQ_STR(old_params.comments, "Comments");
  EXPECT_EQ_STR(old_params.title, "Title");
  EXPECT_EMPTY(old_params.subject);

  // Newer sample: real values; must not return title-cased labels.
  const auto new_sample = resolve_testdata("detailstest.prt.5");
  std::cout << "New sample: " << new_sample << "\n";
  EXPECT_TRUE(fs::exists(new_sample));

  {
    std::ifstream in(new_sample, std::ios::binary);
    EXPECT_TRUE(static_cast<bool>(in));
    std::vector<std::uint8_t> buf((std::istreambuf_iterator<char>(in)),
                                  std::istreambuf_iterator<char>());
    const auto authors =
        creo::extract_string_param(buf.data(), buf.size(), "AUTHORS");
    EXPECT_EQ_STR(authors, "BRO");
    const auto missing =
        creo::extract_string_param(buf.data(), buf.size(), "NO_SUCH_PARAM");
    EXPECT_EMPTY(missing);
  }

  const auto new_params =
      creo::extract_explorer_params_from_file(new_sample.string());
  EXPECT_EQ_STR(new_params.authors, "BRO");
  EXPECT_EQ_STR(new_params.tags, "Product Line");
  EXPECT_EQ_STR(new_params.comments, "Comments");
  EXPECT_EQ_STR(new_params.title, "Descrip line 1");
  EXPECT_EQ_STR(new_params.subject, "Descrip line 2");

  const std::uint8_t junk[] = {'N', 'O', 'T', 'U', 'G', 'C'};
  const auto none = creo::extract_explorer_params(junk, sizeof(junk));
  EXPECT_EMPTY(none.authors);
  EXPECT_EMPTY(none.title);

  if (g_failures == 0) {
    std::cout << "All extractor tests passed.\n";
    return 0;
  }
  std::cerr << g_failures << " failure(s)\n";
  return 1;
}
