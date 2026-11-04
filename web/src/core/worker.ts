// Owns the wasm core. The module is single-threaded and conversions block,
// so it lives here rather than on the UI thread.
import * as Comlink from 'comlink'
import createHanvi, { type MainModule } from './generated/hanvi.js'
import type { Entry, Lookup, NameSet, Paragraph, Priority } from './types'

const DATA_URL = `${import.meta.env.BASE_URL}data/`
const OPENCC_FILES = [
  't2s.json',
  'TSPhrases.ocd2',
  'TSCharacters.ocd2',
  'TSCharactersExt.ocd2',
  'CJK_Compatibility_Ideographs.ocd2',
]

const DB_PATH = '/data/dict.db'
const SNAPSHOT_PATH = '/data/snapshot.db'
const IMPORT_PATH = '/data/import.db'

// The user's dictionary is kept in the origin private file system (OPFS) once it has been
// edited; until then the bundled copy is used. Every browser tab runs its own worker with its
// own in-memory copy, so edits take a lock, first pick up any newer copy another tab saved,
// then save; the stored file's lastModified tells copies apart.
const STORED_NAME = 'dict.db'
const LOCK_NAME = 'hanvi-dictionary'
const channel = new BroadcastChannel('hanvi-dictionary')

let hanvi: MainModule
let ready: Promise<void> | undefined
// null when OPFS is unavailable (e.g. some private windows); edits then last only for this tab.
let storage: FileSystemDirectoryHandle | null = null
let loadedVersion = 0
let savedChanges = 0
let onExternalChange: (() => void) | undefined
let onSaveError: ((message: string) => void) | undefined

async function download(name: string): Promise<Uint8Array<ArrayBuffer>> {
  const response = await fetch(DATA_URL + name)
  if (!response.ok) throw new Error(`Failed to fetch ${name}: ${response.status}`)
  return new Uint8Array(await response.arrayBuffer())
}

async function gunzip(bytes: Uint8Array<ArrayBuffer>): Promise<Uint8Array<ArrayBuffer>> {
  const unpacked = new Blob([bytes]).stream().pipeThrough(new DecompressionStream('gzip'))
  return new Uint8Array(await new Response(unpacked).arrayBuffer())
}

function isSqlite(bytes: Uint8Array): boolean {
  return new TextDecoder().decode(bytes.subarray(0, 16)) === 'SQLite format 3\0'
}

// The database ships gzipped (Cloudflare Pages limits assets to 25 MB). Some servers (vite
// preview, nginx's gzip_static) also send a .gz with Content-Encoding: gzip, and the browser has
// then unpacked it already, so the bytes decide, not the file name. Anything that is not SQLite
// (a single-page-app fallback answering a missing file with index.html, a Git LFS pointer) is
// rejected rather than loaded as an empty dictionary.
async function downloadDb(): Promise<Uint8Array> {
  for (const name of ['dict.db.gz', 'dict.db']) {
    try {
      let bytes = await download(name)
      if (bytes[0] === 0x1f && bytes[1] === 0x8b) bytes = await gunzip(bytes)
      if (isSqlite(bytes)) return bytes
      console.warn(`${name} is not a SQLite database`)
    } catch (err) {
      console.warn(`Could not load ${name}:`, err)
    }
  }
  throw new Error('Could not download the dictionary database (data/dict.db.gz)')
}

async function openStorage(): Promise<FileSystemDirectoryHandle | null> {
  try {
    return await navigator.storage.getDirectory()
  } catch {
    return null
  }
}

async function storedFile(): Promise<File | null> {
  if (!storage) return null
  try {
    return await (await storage.getFileHandle(STORED_NAME)).getFile()
  } catch {
    return null
  }
}

async function writeStored(bytes: Uint8Array<ArrayBuffer>): Promise<number> {
  const handle = await storage!.getFileHandle(STORED_NAME, { create: true })
  const writable = await handle.createWritable()
  await writable.write(bytes)
  await writable.close()
  return (await handle.getFile()).lastModified
}

async function load() {
  storage = await openStorage()
  const stored = await storedFile()
  const [module, db, ...opencc] = await Promise.all([
    createHanvi(),
    stored ? stored.arrayBuffer().then((buffer) => new Uint8Array(buffer)) : downloadDb(),
    ...OPENCC_FILES.map((name) => download(`opencc/${name}`)),
  ])
  module.FS.mkdirTree('/data/opencc', 0o777)
  module.FS.writeFile(DB_PATH, db)
  OPENCC_FILES.forEach((name, i) => module.FS.writeFile(`/data/opencc/${name}`, opencc[i]))

  module.initOpencc('/data/opencc/t2s.json')
  module.loadDict(DB_PATH)
  hanvi = module
  loadedVersion = stored?.lastModified ?? 0
  savedChanges = hanvi.dbChangeCount()
}

// Swaps the open dictionary for another copy.
function replaceDb(bytes: Uint8Array) {
  hanvi.closeDb()
  hanvi.FS.writeFile(DB_PATH, bytes)
  hanvi.reopenDict(DB_PATH)
  savedChanges = hanvi.dbChangeCount()
}

// Loads the stored dictionary if another tab saved a newer one, or the bundled one if another
// tab reset it (the stored copy is gone). Call with the lock held.
async function refresh() {
  const stored = await storedFile()
  if (stored ? stored.lastModified === loadedVersion : loadedVersion === 0) return
  const started = performance.now()
  replaceDb(stored ? new Uint8Array(await stored.arrayBuffer()) : await downloadDb())
  loadedVersion = stored?.lastModified ?? 0
  console.debug(`Loaded the dictionary another tab saved in ${Math.round(performance.now() - started)} ms`)
  onExternalChange?.()
}

// A consistent copy of the open dictionary.
function snapshot(): Uint8Array<ArrayBuffer> {
  hanvi.saveDbCopy(SNAPSHOT_PATH)
  const bytes: Uint8Array<ArrayBuffer> = hanvi.FS.readFile(SNAPSHOT_PATH)
  hanvi.FS.unlink(SNAPSHOT_PATH)
  return bytes
}

// Makes bytes the stored dictionary and tells the other tabs. Call with the lock held.
async function store(bytes: Uint8Array<ArrayBuffer>) {
  const started = performance.now()
  loadedVersion = await writeStored(bytes)
  console.debug(`Saved dictionary (${(bytes.length / 1e6).toFixed(1)} MB) in ${Math.round(performance.now() - started)} ms`)
  channel.postMessage('saved')
}

// Saves the dictionary if it changed. Call with the lock held.
async function persist() {
  if (!storage || hanvi.dbChangeCount() === savedChanges) return
  await store(snapshot())
  savedChanges = hanvi.dbChangeCount()
}

// Applies an edit and resolves as soon as it is in memory. Saving the whole file takes a while,
// so it finishes in the background; it keeps the lock meanwhile, so the next edit here or in
// another tab starts from the saved copy.
// Web Locks, like OPFS, exist only in secure contexts (HTTPS or localhost). Over plain HTTP, e.g.
// trying the app on a phone across the LAN, nothing is stored, so there is nothing to guard.
function withLock<T>(task: () => Promise<T>): Promise<T> {
  return navigator.locks ? navigator.locks.request(LOCK_NAME, task) : task()
}

async function edit<T>(change: () => T): Promise<T> {
  await api.init()
  return new Promise<T>((resolve, reject) => {
    void withLock(async () => {
      try {
        await refresh()
        resolve(change())
      } catch (error) {
        reject(error)
        return
      }
      try {
        await persist()
      } catch (error) {
        onSaveError?.(String(error))
      }
    })
  })
}

// Reads wait for loading, since the UI can ask before the dictionaries are ready.
async function read<T>(query: () => T): Promise<T> {
  await api.init()
  return query()
}

channel.onmessage = () => {
  if (hanvi) void withLock(refresh)
}

const api = {
  init: () => (ready ??= load()),
  // Whether edits are saved across reloads.
  hasStorage: async () => {
    await api.init()
    return storage !== null
  },
  // Called after another tab's edits were loaded, so the UI can re-convert.
  onExternalChange: (callback: () => void) => {
    onExternalChange = callback
  },
  onSaveError: (callback: (message: string) => void) => {
    onSaveError = callback
  },
  // Whether edits are saved across reloads, and whether the dictionary differs from the bundled one.
  storageInfo: async () => {
    await api.init()
    return { available: storage !== null, customized: (await storedFile()) !== null }
  },

  // The whole dictionary as a SQLite file, the same format as the desktop's dict.db. Taken after
  // picking up other tabs' edits.
  exportDb: async () => {
    await api.init()
    const bytes = await withLock(async () => {
      await refresh()
      return snapshot()
    })
    return Comlink.transfer(bytes, [bytes.buffer])
  },
  // Replaces the dictionary with a dict.db file, once core has checked it is one.
  importDb: async (bytes: Uint8Array<ArrayBuffer>) => {
    await api.init()
    if (!isSqlite(bytes)) throw new Error('Not imported: the file is not a SQLite database.')
    await withLock(async () => {
      hanvi.FS.writeFile(IMPORT_PATH, bytes)
      const valid = hanvi.isDictionaryDb(IMPORT_PATH)
      hanvi.FS.unlink(IMPORT_PATH)
      if (!valid) throw new Error('Not imported: the file is not a Hanvi dictionary, or it is damaged.')
      replaceDb(bytes)
      if (!storage) return
      try {
        await store(bytes)
      } catch (error) {
        throw new Error(`Imported for this tab only: the browser could not store it (${error}).`)
      }
    })
  },
  // Goes back to the bundled dictionary, dropping the stored one with all edits.
  resetDb: async () => {
    await api.init()
    await withLock(async () => {
      replaceDb(await downloadDb())
      if (!storage) return
      try {
        await storage.removeEntry(STORED_NAME)
      } catch (error) {
        if (!(error instanceof DOMException && error.name === 'NotFoundError')) throw error
      }
      loadedVersion = 0
      channel.postMessage('saved')
    })
  },

  convert: (text: string) => read((): Paragraph[] => hanvi.convert(hanvi.normalize(text))),
  convertPlain: (text: string) => read((): string => hanvi.convertPlain(hanvi.normalize(text))),
  reloadDict: () => read(() => hanvi.reloadDict()),

  nameSets: () => read((): NameSet[] => hanvi.nameSets()),
  activeNameSet: () => read(() => hanvi.activeNameSet()),
  loadNameSet: (id: number) => read(() => hanvi.loadNameSet(id)),
  nameSetEntryCounts: () => read((): Record<number, number> => hanvi.nameSetEntryCounts()),

  lookup: (key: string) => read((): Lookup => hanvi.lookup(key)),
  listEntries: (setId: number, priority: Priority, filter: string, offset: number, limit: number) =>
    read((): Entry[] => hanvi.listEntries(setId, priority, filter, offset, limit)),
  countEntries: (setId: number, priority: Priority) => read(() => hanvi.countEntries(setId, priority)),
  getTranslation: (key: string, priority: Priority) =>
    read((): string | null => hanvi.getTranslation(key, priority)),

  insertEntry: (setId: number, key: string, value: string, priority: Priority) =>
    edit(() => hanvi.insertEntry(setId, key, value, priority)),
  removeEntry: (setId: number, key: string, priority: Priority) =>
    edit(() => hanvi.removeEntry(setId, key, priority)),
  removeMeaning: (key: string, value: string) => edit(() => hanvi.removeMeaning(key, value)),
  reorderMeanings: (key: string, order: string[]) => edit(() => hanvi.reorderMeanings(key, order)),

  // Nameset ids, or null when the title is taken (or an import holds duplicate keys).
  createNameSet: (title: string) => edit(() => orNull(hanvi.createNameSet(title))),
  importNameSet: (title: string, entries: Entry[]) => edit(() => orNull(hanvi.importNameSet(title, entries))),
  renameNameSet: (id: number, title: string) => edit(() => hanvi.renameNameSet(id, title)),
  deleteNameSet: (id: number) => edit(() => hanvi.deleteNameSet(id)),
}

function orNull(id: number): number | null {
  return id === -1 ? null : id
}

export type CoreApi = typeof api

Comlink.expose(api)
