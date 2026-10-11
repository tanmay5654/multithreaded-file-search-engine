#include "search_engine.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {
void usage(const char* program) {
    std::cerr << "Usage: " << program << " <directory> [--threads N] [--limit N]\n"
              << "Index a directory, then enter search queries (Ctrl-D to exit).\n";
}
}

int main(int argc, char** argv) {
    if (argc < 2) { usage(argv[0]); return 2; }
    std::size_t threads = 0, limit = 10;
    for (int i = 2; i < argc; ++i) {
        const std::string option = argv[i];
        if ((option == "--threads" || option == "--limit") && i + 1 < argc) {
            char* end = nullptr;
            const auto value = std::strtoull(argv[++i], &end, 10);
            if (!end || *end != '\0' || value == 0) { usage(argv[0]); return 2; }
            if (option == "--threads") threads = static_cast<std::size_t>(value);
            else limit = static_cast<std::size_t>(value);
        } else { usage(argv[0]); return 2; }
    }

    try {
        search::SearchEngine engine;
        const auto stats = engine.index(argv[1], threads);
        std::cout << "Indexed " << stats.files_indexed << '/' << stats.files_seen
                  << " files; " << stats.unique_terms << " unique terms.\n";
        std::string query;
        while (std::cout << "search> " && std::getline(std::cin, query)) {
            for (const auto& result : engine.search(query, limit))
                std::cout << result.score << "\t" << result.path.string() << '\n';
        }
    } catch (const std::exception& error) {
        std::cerr << "file_search: " << error.what() << '\n';
        return 1;
    }
}
