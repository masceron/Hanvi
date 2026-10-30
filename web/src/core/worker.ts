// Owns the wasm core. The module is single-threaded and conversions block,
// so it lives here rather than on the UI thread.
import * as Comlink from 'comlink'
import createHanvi, { type MainModule } from './generated/hanvi.js'
import type { NameSet, Paragraph } from './types'

const DATA_URL = `${import.meta.env.BASE_URL}data/`
const OPENCC_FILES = [
  't2s.json',
  'TSPhrases.ocd2',
  'TSCharacters.ocd2',
  'TSCharactersExt.ocd2',
  'CJK_Compatibility_Ideographs.ocd2',
]

let hanvi: MainModule
let ready: Promise<void> | undefined

async function download(name: string): Promise<Uint8Array> {
  const response = await fetch(DATA_URL + name)
  if (!response.ok) throw new Error(`Failed to fetch ${name}: ${response.status}`)
  return new Uint8Array(await response.arrayBuffer())
}

async function load() {
  const [module, db, ...opencc] = await Promise.all([
    createHanvi(),
    download('dict.db'),
    ...OPENCC_FILES.map((name) => download(`opencc/${name}`)),
  ])
  module.FS.mkdirTree('/data/opencc', 0o777)
  module.FS.writeFile('/data/dict.db', db)
  OPENCC_FILES.forEach((name, i) => module.FS.writeFile(`/data/opencc/${name}`, opencc[i]))

  module.initOpencc('/data/opencc/t2s.json')
  module.loadDict('/data/dict.db')
  hanvi = module
}

const api = {
  init: () => (ready ??= load()),

  convert: (text: string): Paragraph[] => hanvi.convert(hanvi.normalize(text)),
  convertPlain: (text: string): string => hanvi.convertPlain(hanvi.normalize(text)),

  nameSets: (): NameSet[] => hanvi.nameSets(),
  activeNameSet: (): number => hanvi.activeNameSet(),
  loadNameSet: (id: number) => hanvi.loadNameSet(id),
}

export type CoreApi = typeof api

Comlink.expose(api)
