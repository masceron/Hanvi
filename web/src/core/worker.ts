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

async function download(name: string): Promise<Uint8Array> {
  const response = await fetch(DATA_URL + name)
  if (!response.ok) throw new Error(`Failed to fetch ${name}: ${response.status}`)
  return new Uint8Array(await response.arrayBuffer())
}

async function downloadDb(): Promise<Uint8Array> {
  try {
    const response = await fetch(DATA_URL + 'dict.db.gz')
    if (response.ok && response.body) {
      const ds = new DecompressionStream('gzip')
      const decompressed = response.body.pipeThrough(ds)
      return new Uint8Array(await new Response(decompressed).arrayBuffer())
    }
  } catch (err) {
    console.warn('Could not load dict.db.gz, falling back to dict.db:', err)
  }
  return download('dict.db')
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

// Loads the stored dictionary if another tab saved a newer one. Call with the lock held.
async function refresh() {
  const stored = await storedFile()
  if (!stored || stored.lastModified === loadedVersion) return
  const started = performance.now()
  const bytes = new Uint8Array(await stored.arrayBuffer())
  hanvi.closeDb()
  hanvi.FS.writeFile(DB_PATH, bytes)
  hanvi.reopenDict(DB_PATH)
  loadedVersion = stored.lastModified
  savedChanges = hanvi.dbChangeCount()
  console.debug(`Loaded the dictionary another tab saved in ${Math.round(performance.now() - started)} ms`)
  onExternalChange?.()
}

// Saves the dictionary if it changed. Call with the lock held.
async function persist() {
  if (!storage || hanvi.dbChangeCount() === savedChanges) return
  hanvi.saveDbCopy(SNAPSHOT_PATH)
  const bytes: Uint8Array<ArrayBuffer> = hanvi.FS.readFile(SNAPSHOT_PATH)
  hanvi.FS.unlink(SNAPSHOT_PATH)
  const started = performance.now()
  loadedVersion = await writeStored(bytes)
  savedChanges = hanvi.dbChangeCount()
  console.debug(`Saved dictionary (${(bytes.length / 1e6).toFixed(1)} MB) in ${Math.round(performance.now() - started)} ms`)
  channel.postMessage('saved')
}

// Applies an edit and resolves as soon as it is in memory. Saving the whole file takes a while,
// so it finishes in the background; it keeps the lock meanwhile, so the next edit here or in
// another tab starts from the saved copy.
async function edit<T>(change: () => T): Promise<T> {
  await api.init()
  return new Promise<T>((resolve, reject) => {
    void navigator.locks.request(LOCK_NAME, async () => {
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
  if (hanvi) void navigator.locks.request(LOCK_NAME, refresh)
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
