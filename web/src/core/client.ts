import * as Comlink from 'comlink'
import type { CoreApi } from './worker'

export type { NameSet, Paragraph, Token } from './types'

export const core = Comlink.wrap<CoreApi>(
  new Worker(new URL('./worker.ts', import.meta.url), { type: 'module' }),
)
