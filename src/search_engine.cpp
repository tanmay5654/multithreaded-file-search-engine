#include "search_engine.hpp"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cmath>
#include <fstream>
#include <mutex>
#include <stdexcept>
#include <thread>

namespace search {
namespace {

std::vector<std::string> tokenize(const std::string& text) {
    std::vector<std::string> words;
    std::string word;
    for (unsigned char ch : text) {
        if (std::isalnum(ch)) {
            word.push_back(static_cast<char>(std::tolower(ch)));
        } else if (!word.empty()) {
            words.push_back(std::move(word));
            word.clear();
        }
    }
    if (!word.empty()) words.push_back(std::move(word));
    return words;
}

struct ParsedDocument {
    std::filesystem::path path;
    std::unordered_map<std::string, std::size_t> frequencies;
    std::size_t length{};
};

} // namespace

IndexStats SearchEngine::index(const std::filesystem::path& root,
                               std::size_t worker_count) {
    if (!std::filesystem::exists(root))
        throw std::filesystem::filesystem_error("Index root does not exist", root,
                                                std::make_error_code(std::errc::no_such_file_or_directory));
    if (!std::filesystem::is_directory(root))
        throw std::invalid_argument("Index root must be a directory: " + root.string());

    std::vector<std::filesystem::path> paths;
    std::error_code ec;
    for (std::filesystem::recursive_directory_iterator it(
             root, std::filesystem::directory_options::skip_permission_denied, ec), end;
         it != end; it.increment(ec)) {
        if (ec) { ec.clear(); continue; }
        std::error_code type_error;
        if (it->is_regular_file(type_error) && !type_error) paths.push_back(it->path());
    }

    std::vector<ParsedDocument> parsed(paths.size());
    std::atomic<std::size_t> next{0};
    std::atomic<std::size_t> skipped{0};
    const auto available = std::max(1u, std::thread::hardware_concurrency());
    const auto count = std::min(paths.size(), worker_count == 0 ? available : worker_count);
    std::vector<std::thread> workers;
    workers.reserve(count);

    for (std::size_t w = 0; w < count; ++w) {
        workers.emplace_back([&] {
            for (;;) {
                const auto i = next.fetch_add(1, std::memory_order_relaxed);
                if (i >= paths.size()) break;
                std::ifstream input(paths[i], std::ios::binary);
                if (!input) { ++skipped; continue; }
                std::string contents((std::istreambuf_iterator<char>(input)),
                                     std::istreambuf_iterator<char>());
                auto tokens = tokenize(contents);
                auto& doc = parsed[i];
                doc.path = paths[i];
                doc.length = tokens.size();
                for (const auto& token : tokens) ++doc.frequencies[token];
            }
        });
    }
    for (auto& worker : workers) worker.join();

    clear();
    documents_.reserve(paths.size() - skipped.load());
    for (auto& doc : parsed) {
        if (doc.path.empty()) continue;
        const auto id = documents_.size();
        documents_.push_back({std::move(doc.path), doc.length});
        for (const auto& [term, frequency] : doc.frequencies)
            inverted_index_[term].emplace(id, Posting{frequency, doc.length});
    }
    return {paths.size(), documents_.size(), skipped.load(), inverted_index_.size()};
}

std::vector<SearchResult> SearchEngine::search(const std::string& query,
                                               std::size_t limit) const {
    if (limit == 0) return {};
    const auto terms = tokenize(query);
    std::unordered_map<std::size_t, SearchResult> ranked;
    const double document_count = static_cast<double>(documents_.size());
    double average_document_length = 0.0;
    if (!documents_.empty()) {
        for (const auto& doc : documents_)
            average_document_length += static_cast<double>(doc.length);
        average_document_length /= document_count;
    }

    for (const auto& term : terms) {
        const auto found = inverted_index_.find(term);
        if (found == inverted_index_.end()) continue;
        const double df = static_cast<double>(found->second.size());
        const double idf = std::log(1.0 + (document_count - df + 0.5) / (df + 0.5));
        for (const auto& [id, posting] : found->second) {
            auto& result = ranked[id];
            result.path = documents_[id].path;
            ++result.matching_terms;
            const double tf = static_cast<double>(posting.term_frequency);
            const double dl = static_cast<double>(posting.document_length);
            constexpr double k1 = 1.2, b = 0.75;
            result.score += idf * (tf * (k1 + 1.0)) /
                (tf + k1 * (1.0 - b + b *
                    (average_document_length == 0.0 ? 0.0 : dl / average_document_length)));
        }
    }

    std::vector<SearchResult> results;
    results.reserve(ranked.size());
    for (auto& [id, result] : ranked) { (void)id; results.push_back(std::move(result)); }
    std::sort(results.begin(), results.end(), [](const auto& a, const auto& b) {
        if (a.score != b.score) return a.score > b.score;
        return a.path.string() < b.path.string();
    });
    if (results.size() > limit) results.resize(limit);
    return results;
}

void SearchEngine::clear() {
    inverted_index_.clear();
    documents_.clear();
}

} // namespace search
