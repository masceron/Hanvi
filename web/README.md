# Hanvi Web

The browser frontend: the core compiled to WebAssembly, running in a Web Worker, behind a React + TypeScript UI built with Vite.

```
web/
├── wasm/             embind bindings over the core (C++, built by CMake)
└── src/
    ├── core/
    │   ├── worker.ts     loads the module and data, exposes the API with Comlink
    │   ├── client.ts     `core` proxy the UI awaits (`await core.convert(text)`)
    │   ├── types.ts      shapes returned by the bindings
    │   └── generated/    hanvi.js / hanvi.wasm / hanvi.d.ts (build output, ignored)
    └── App.tsx
```

The module is single-threaded, so the page needs no cross-origin isolation headers; it runs in a worker so loading and long conversions don't block the UI.

## Building

Prerequisites: [emsdk](https://emscripten.org/docs/getting_started/downloads.html), Node.js, CMake, and a host C++ compiler (Ninja, Clang, GCC, or MSVC).

1. Once per emsdk install, give Emscripten its own TypeScript compiler (used for `hanvi.d.ts`):

   ```sh
   (cd "$EMSDK/upstream/emscripten" && npm ci)
   ```

2. Build and run:

   ```sh
   npm install
   npm run build     # compiles the core (HanviWasm), runs tsc, and bundles dist/
   npm run preview   # or: npm run dev
   ```

   To rebuild only the C++ core and bindings during development without rebuilding the frontend:

   ```sh
   npm run build:core
   ```

## Notes

- `dict.db` is about 40 MB uncompressed. During the build, it is automatically compressed to `dict.db.gz` (~14 MB) to satisfy Cloudflare Pages' 25MB single-asset limit and is decompressed on the fly via the browser's native `DecompressionStream('gzip')`.
- Dictionary edits are saved in the browser's origin private file system (OPFS). Until the first edit the bundled `dict.db` is used; after it, the stored copy is loaded instead, so a newer bundled `dict.db` does not replace it.
- Each browser tab has its own in-memory copy. An edit takes a Web Lock, first loads any copy another tab saved since (by the file's `lastModified`), applies the change, and saves the whole file (about 65 ms for 41 MB); a `BroadcastChannel` message makes the other tabs reload it (about 0.5 s) and re-convert.
- Importing and exporting the whole `dict.db` is not there yet.
