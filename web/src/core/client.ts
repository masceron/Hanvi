import * as Comlink from 'comlink'
import { decodeDocument } from './packed'
import type { Paragraph } from './types'
import type { CoreApi } from './worker'

export type { DocumentState, NameSet, Paragraph, Token } from './types'

export const core = Comlink.wrap<CoreApi>(
  new Worker(new URL('./worker.ts', import.meta.url), { type: 'module' }),
)

export async function convert(text: string): Promise<Paragraph[]> {
  return decodeDocument(await core.convert(text))
}
