#include "MappingConfig.h"
#include "creo_param_extractor.hpp"

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

namespace {

std::vector<std::uint8_t> read_all(const char* path) {
  std::ifstream in(path, std::ios::binary);
  return std::vector<std::uint8_t>((std::istreambuf_iterator<char>(in)), {});
}

std::vector<std::uint8_t> make_padded(const std::vector<std::uint8_t>& base,
                                      std::size_t target_size) {
  std::vector<std::uint8_t> out = base;
  if (out.size() >= target_size) {
    return out;
  }
  // Pad with non-NUL bytes so the scan cost reflects typical binary density
  // (a zero-filled tail would create an artificial NUL at every offset).
  out.resize(target_size, 0xA5);
  return out;
}

struct Stats {
  double wall_ms = 0;
  double per_file_us = 0;
  std::uint64_t bytes = 0;
};

Stats time_loop(int n, const std::function<void(std::uint64_t&)>& body) {
  std::uint64_t bytes = 0;
  const auto t0 = std::chrono::steady_clock::now();
  for (int i = 0; i < n; ++i) {
    body(bytes);
  }
  const auto t1 = std::chrono::steady_clock::now();
  Stats s;
  s.wall_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
  s.per_file_us = (s.wall_ms * 1000.0) / static_cast<double>(n);
  s.bytes = bytes;
  return s;
}

void print_stats(const char* label, int n, const Stats& s,
                 std::size_t file_size, std::size_t params) {
  std::cout << label << " N=" << n << " wall_ms=" << s.wall_ms
            << " per_file_us=" << s.per_file_us << " bytes=" << s.bytes
            << " file_size=" << file_size << " params=" << params << "\n";
}

}  // namespace

int main(int argc, char** argv) {
  const char* path =
      argc > 1 ? argv[1] : "/workspace/testdata/detailstest.prt.5";
  const int n_small = argc > 2 ? std::atoi(argv[2]) : 1000;
  const int n_large = argc > 3 ? std::atoi(argv[3]) : 5000;

  const auto buf = read_all(path);
  if (buf.empty() || !creo::is_creo_ugc_file(buf.data(), buf.size())) {
    std::cerr << "failed to load UGC sample: " << path << "\n";
    return 1;
  }

  auto map_text = mapping::read_text_file(
      "/workspace/native/CreoExplorerHandler/CreoExplorerHandler.map");
  mapping::Config cfg =
      map_text ? mapping::parse_config_text(*map_text) : mapping::default_config();
  mapping::resolve_entries(cfg);

  std::vector<std::string> map_params;
  for (const auto& e : cfg.resolved) {
    map_params.push_back(e.creo_param);
  }
  const std::vector<std::string> sample_params = {"AUTHORS", "TAGS", "COMMENTS",
                                                  "TITLE", "SUBJECT"};

  // Correctness smoke: multi-extract matches single-extract for sample params.
  {
    const auto multi =
        creo::extract_string_params(buf.data(), buf.size(), sample_params);
    for (const auto& name : sample_params) {
      const auto single =
          creo::extract_string_param(buf.data(), buf.size(), name);
      const auto it = multi.find(name);
      if (single) {
        if (it == multi.end() || it->second != *single) {
          std::cerr << "mismatch for " << name << "\n";
          return 1;
        }
      } else if (it != multi.end()) {
        std::cerr << "unexpected multi hit for " << name << "\n";
        return 1;
      }
    }
  }

  auto run_n = [&](int n) {
    std::cout << "---- N=" << n << " ----\n";

    auto s1 = time_loop(n, [&](std::uint64_t& bytes) {
      auto text = mapping::read_text_file(
          "/workspace/native/CreoExplorerHandler/CreoExplorerHandler.map");
      auto c = text ? mapping::parse_config_text(*text)
                    : mapping::default_config();
      bytes += text ? text->size() : 0;
      for (const auto& e : c.entries) {
        auto v =
            creo::extract_string_param(buf.data(), buf.size(), e.creo_param);
        (void)v;
        bytes += buf.size();
      }
    });
    print_stats("baseline_reload+N_scans", n, s1, buf.size(),
                cfg.entries.size());

    auto s2 = time_loop(n, [&](std::uint64_t& bytes) {
      for (const auto& name : map_params) {
        auto v = creo::extract_string_param(buf.data(), buf.size(), name);
        (void)v;
        bytes += buf.size();
      }
    });
    print_stats("baseline_N_scans_no_reload", n, s2, buf.size(),
                map_params.size());

    auto s3 = time_loop(n, [&](std::uint64_t& bytes) {
      auto values =
          creo::extract_string_params(buf.data(), buf.size(), map_params);
      bytes += buf.size();
      (void)values;
    });
    print_stats("optimized_single_pass", n, s3, buf.size(), map_params.size());

    auto s4 = time_loop(n, [&](std::uint64_t& bytes) {
      auto values =
          creo::extract_string_params(buf.data(), buf.size(), sample_params);
      bytes += buf.size();
      (void)values;
    });
    print_stats("optimized_sample_5params", n, s4, buf.size(),
                sample_params.size());

    // Cached map path: parse once outside loop (mtime cache simulation).
    auto s5 = time_loop(n, [&](std::uint64_t& bytes) {
      auto values =
          creo::extract_string_params(buf.data(), buf.size(), map_params);
      bytes += buf.size();
      for (const auto& e : cfg.resolved) {
        auto it = values.find(e.creo_param);
        (void)it;
        (void)e;
      }
    });
    print_stats("optimized_cached_map+single_pass", n, s5, buf.size(),
                map_params.size());
  };

  run_n(n_small);
  run_n(n_large);

  // Synthetic larger buffer (same params embedded, padded) to show scan bound.
  const auto padded = make_padded(buf, 4ull * 1024ull * 1024ull);
  auto sbig = time_loop(200, [&](std::uint64_t& bytes) {
    auto values =
        creo::extract_string_params(padded.data(), padded.size(), map_params);
    bytes += std::min(padded.size(), creo::kMaxParamScanBytes);
    (void)values;
  });
  print_stats("optimized_4MiB_padded_N200", 200, sbig, padded.size(),
              map_params.size());

  std::cout << "stress_hotpath: OK\n";
  return 0;
}
