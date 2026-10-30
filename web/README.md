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

Prerequisites: [emsdk](https://emscripten.org/docs/getting_started/downloads.html), Node.js, and CMake. (Host OpenCC dictionary tools are built automatically if missing).

1. Once per emsdk install, install Emscripten's TypeScript compiler:

   ```sh
   (cd "$EMSDK/upstream/emscripten" && npm ci)
   ```

2. Build and run:

   ```sh
   npm install
   npm run build     # compiles the core (HanviWasm), runs tsc, and bundles dist/
   npm run preview   # or: npm run dev
   ```

   To rebuild only the C++ core and bindings during development:

   ```sh
   npm run build:core
   ```

## Notes

- `dict.db` is compressed to `dict.db.gz` (~14 MB) during build to stay within Cloudflare Pages' 25MB asset limit, and decompressed on load via `DecompressionStream('gzip')`.
- Dictionary edits are saved in the browser's origin private file system (OPFS).
- Each browser tab has its own in-memory copy, kept in sync via Web Locks and `BroadcastChannel`.
