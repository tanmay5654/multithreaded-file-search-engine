#include "search_engine.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>

int main() {
    namespace fs = std::filesystem;
    const auto root = fs::temp_directory_path() / "file_search_engine_test_data";
    fs::remove_all(root);
    fs::create_directories(root / "nested");
    { std::ofstream(root / "alpha.txt") << "apple banana apple"; }
    { std::ofstream(root / "nested" / "beta.txt") << "banana carrot"; }

    search::SearchEngine engine;
    const auto stats = engine.index(root, 2);
    assert(stats.files_seen == 2 && stats.files_indexed == 2);
    assert(stats.unique_terms == 3);
    const auto results = engine.search("APPLE banana");
    assert(results.size() == 2);
    assert(results.front().path.filename() == "alpha.txt");
    assert(results.front().matching_terms == 2);
    assert(engine.search("missing").empty());

    fs::remove_all(root);
}
