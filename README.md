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

### Profile-guided optimization (optional, Clang only)
Trains on your own texts, so it is not part of a default build. An instrumented build of the benchmark records a profile, and a release build optimizes with it:

```bash
cmake -B build-pgo -S . -DCMAKE_BUILD_TYPE=Release -DHANVI_PGO_TRAINING=path/to/novels
cmake --build build-pgo --target PgoProfile    # writes build-pgo/hanvi.profdata
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=[YOUR_QT_PATH] -DHANVI_PGO_PROFILE=build-pgo/hanvi.profdata
cmake --build build
```
