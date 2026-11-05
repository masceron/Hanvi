# Hanvi
A Chinese-Vietnamese conversion tool inspired by QuickTranslator, designed to function as a drop-in replacement.

## Components
* **Hanvi**: The desktop application (Qt 6). The UI mimics QuickTranslator with integrated tools for editing phrases and names.
* **HanviCLI**: A command-line tool optimized for parallel batch conversions.
* **Hanvi Web**: In-browser client running the C++ core via WebAssembly in a Web Worker, paired with a React UI. See https://hanvi-app.pages.dev/

## Compiling

### Desktop & CLI
Requires Qt 6 and a C++23 compiler.

```bash
cmake -B build -S . -DCMAKE_PREFIX_PATH=[YOUR_QT_PATH]
cmake --build build --config Release
```

### Web
Requires Node.js, CMake, and [emsdk](https://emscripten.org/docs/getting_started/downloads.html).

```bash
cd web
npm install
npm run build     # builds WASM core, tsc, and bundles web/dist
npm run preview   # or: npm run dev
```

See [web/README.md](web/README.md) for architecture and deployment details.

### Benchmark
`HanviBench` times loading, normalization and both conversions over your own UTF-8 texts, to compare compilers and compile options. It needs no Qt.

```bash
cmake -B build-bench -S . -DHANVI_BUILD_BENCH=ON -DHANVI_BUILD_DESKTOP=OFF -DHANVI_BUILD_CLI=OFF
cmake --build build-bench --config Release
build-bench/HanviBench --rounds 5 path/to/novels   # folders are searched for .txt files
```

