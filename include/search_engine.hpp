#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace search {

struct SearchResult {
    std::filesystem::path path;
    double score{};
    std::size_t matching_terms{};
};

struct IndexStats {
    std::size_t files_seen{};
    std::size_t files_indexed{};
    std::size_t files_skipped{};
    std::size_t unique_terms{};
};

class SearchEngine {
public:
    // Recursively indexes regular files. A zero worker count selects hardware concurrency.
    IndexStats index(const std::filesystem::path& root, std::size_t worker_count = 0);
    std::vector<SearchResult> search(const std::string& query,
                                     std::size_t limit = 10) const;
    void clear();

private:
    struct Posting {
        std::size_t term_frequency{};
        std::size_t document_length{};
    };
    struct Document {
        std::filesystem::path path;
        std::size_t length{};
    };

    std::unordered_map<std::string,
        std::unordered_map<std::size_t, Posting>> inverted_index_;
    std::vector<Document> documents_;
};

} // namespace search
