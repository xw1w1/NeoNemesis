#include <benchmark/benchmark.h>

#include <Windows.h>

#include <cstdint>
#include <filesystem>
#include <format>
#include <string>

#include "omni/hash.hpp"
#include "omni/module.hpp"
#include "omni/modules.hpp"

namespace {

  enum class export_position {
    first,
    last,
    missing,
  };

  class fixture_module {
   public:
    explicit fixture_module(std::wstring_view path) noexcept: handle_(LoadLibraryW(path.data())) {}

    fixture_module(const fixture_module&) = delete;
    fixture_module& operator=(const fixture_module&) = delete;

    fixture_module(fixture_module&& other) noexcept: handle_(std::exchange(other.handle_, nullptr)) {}

    fixture_module& operator=(fixture_module&& other) noexcept {
      if (this != &other) {
        reset();
        handle_ = std::exchange(other.handle_, nullptr);
      }

      return *this;
    }

    ~fixture_module() noexcept {
      reset();
    }

    [[nodiscard]] HMODULE native_handle() const noexcept {
      return handle_;
    }

    [[nodiscard]] bool present() const noexcept {
      return handle_ != nullptr;
    }

   private:
    void reset() noexcept {
      if (handle_ != nullptr) {
        FreeLibrary(handle_);
        handle_ = nullptr;
      }
    }

    HMODULE handle_{};
  };

  [[nodiscard]] std::wstring fixture_module_path(std::int64_t named_export_count) {
    const auto file_name = std::format("omni_benchmark_exports_{}.dll", named_export_count);
    const auto path = std::filesystem::path{OMNI_BENCHMARK_FIXTURE_DIRECTORY} / file_name;

    return path.wstring();
  }

  [[nodiscard]] std::string export_name(std::int64_t named_export_count, export_position position) {
    std::int64_t export_index{};

    switch (position) {
    case export_position::first:
      export_index = 0;
      break;
    case export_position::last:
      export_index = named_export_count - 1;
      break;
    case export_position::missing:
      return "omni_benchmark_export_missing";
    }

    return std::format("omni_bm_export_{:04}", export_index);
  }

  [[nodiscard]] std::int64_t expected_scanned_exports(std::int64_t named_export_count, export_position position) noexcept {
    switch (position) {
    case export_position::first:
      return 1;
    case export_position::last:
    case export_position::missing:
      return named_export_count;
    }

    return named_export_count;
  }

  [[nodiscard]] bool expected_lookup_result(const auto& exports, const auto& export_iterator,
    export_position position) noexcept {
    if (position == export_position::missing) {
      return export_iterator == exports.end();
    }

    return export_iterator != exports.end();
  }

  void module_exports_find(benchmark::State& state, export_position position) {
    const auto named_export_count = state.range(0);
    const auto path = fixture_module_path(named_export_count);

    fixture_module loaded_module{path};
    if (!loaded_module.present()) {
      state.SkipWithError("failed to load benchmark fixture module");
      return;
    }

    const auto module = omni::get_module(omni::address{loaded_module.native_handle()});

    if (!module.present()) {
      state.SkipWithError("benchmark fixture module was not found by omni");
      return;
    }

    const auto exports = module.named_exports();
    const auto name = export_name(named_export_count, position);
    const auto name_hash = omni::hash<omni::default_hash>(name);
    const auto export_iterator = exports.find(name_hash);

    if (!expected_lookup_result(exports, export_iterator, position)) {
      state.SkipWithError("fixture export lookup produced unexpected result");
      return;
    }

    for (auto _ : state) {
      auto found_export_iterator = exports.find(name_hash);
      benchmark::DoNotOptimize(found_export_iterator);
    }

    state.counters["named_exports"] = static_cast<double>(named_export_count);
    state.counters["expected_scanned_exports"] = static_cast<double>(expected_scanned_exports(named_export_count, position));
  }

  void module_exports_find_first(benchmark::State& state) {
    module_exports_find(state, export_position::first);
  }

  void module_exports_find_last(benchmark::State& state) {
    module_exports_find(state, export_position::last);
  }

  void module_exports_find_missing(benchmark::State& state) {
    module_exports_find(state, export_position::missing);
  }

  void apply_export_counts(benchmark::Benchmark* benchmark) {
    benchmark->Arg(16)->Arg(64)->Arg(1024)->Arg(4096)->ArgName("named_exports")->MinWarmUpTime(0.1)->MinTime(0.5);
  }

  void apply_first_export_count(benchmark::Benchmark* benchmark) {
    benchmark->Arg(16)->ArgName("named_exports")->MinWarmUpTime(0.1)->MinTime(0.5);
  }

} // namespace

BENCHMARK(module_exports_find_first)->Apply(apply_first_export_count);
BENCHMARK(module_exports_find_last)->Apply(apply_export_counts);
BENCHMARK(module_exports_find_missing)->Apply(apply_export_counts);

BENCHMARK_MAIN();
