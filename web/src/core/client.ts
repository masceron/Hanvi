import * as Comlink from 'comlink'
import { loadFrequencySegmentation } from '../segmentation'
import { decodeDocument } from './packed'
import type { Paragraph, TokenOverride } from './types'
import type { CoreApi } from './worker'

export type { DocumentState, NameSet, Paragraph, Token, TokenOverride } from './types'

export const core = Comlink.wrap<CoreApi>(
  new Worker(new URL('./worker.ts', import.meta.url), { type: 'module' }),
)
// Sent first, so the worker has it before the first conversion it is asked for.
void core.setFrequencySegmentation(loadFrequencySegmentation())

export interface Converted {
  paragraphs: Paragraph[]
  // The overrides passed in that lie in the text but no longer match a token there.
  stale: TokenOverride[]
}

export async function convert(text: string, overrides: TokenOverride[], textOffset: number): Promise<Converted> {
  const packed = await core.convert(text, overrides, textOffset)
  return { paragraphs: decodeDocument(packed), stale: packed.stale.map((index) => overrides[index]) }
}
