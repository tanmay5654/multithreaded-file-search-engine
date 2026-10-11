# Multithreaded File Search Engine

A C++17 command-line search engine that recursively indexes regular files with a
thread pool of workers and searches an in-memory inverted index. Results use a
BM25-style relevance score; tokens are case-insensitive alphanumeric words.

## Build

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

## Use

```sh
./build/file_search ./directory/to/index --threads 8 --limit 20
```

Enter a query at the prompt. Use Ctrl-D to exit. Omit `--threads` to use the
hardware concurrency reported by the system. Files that cannot be opened are
counted as skipped; directory traversal skips permission-denied entries.

## Browser interface

Open `web/index.html` in a modern browser and choose a folder. The page indexes
text locally in the browser and displays ranked results with matching snippets.
It does not upload selected files. Browser folder selection is not supported by
every browser. The browser demo skips files larger than 8 MiB and likely binary
files. It is a standalone UI demonstration; it currently has its own browser
indexer and does not call the C++ engine.

## Tests

```sh
ctest --test-dir build --output-on-failure
```

The tests use CTest and standard C++ assertions, so the project builds without
downloading third-party dependencies.
