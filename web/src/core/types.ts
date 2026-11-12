// Shapes produced by web/wasm/bindings.cpp; embind types them as `any`.

export interface Token {
  id: number
  cn: string
  sv: string
  vn: string
  // Whether a space goes before the token's text in each language's layout.
  cnSpace: boolean
  svSpace: boolean
  vnSpace: boolean
  isRule: boolean
  rule?: Rule
}

export interface Rule {
  originalStart: string
  originalEnd: string
  translationStart: string
  translationEnd: string
}

export type Paragraph = Token[]

export interface NameSet {
  id: number
  title: string
}

// core's Priority enum.
export const Priority = { Phrase: 1, Name: 2 } as const
export type Priority = (typeof Priority)[keyof typeof Priority]

// Nameset id that means the global dictionary.
export const GLOBAL = -1

export interface Entry {
  original: string
  translated: string
}

// An edit in core's history (core/history.h): one key's value before and after, null when absent.
export interface HistoryAction {
  id: number
  time: number // Unix seconds
  kind: 'name' | 'phrase' | 'variant'
  setId: number
  original: string
  before: string | null
  after: string | null
  change: 'add' | 'delete' | 'edit' | 'reorder' | 'addMeaning' | 'removeMeaning'
  // The meaning added or removed.
  meaning: string
  state: UndoState
  // The latest action that changed the key again, for 'changedLater'.
  changedBy: number
}

export type UndoState = 'undoable' | 'undone' | 'changedLater' | 'changedOutside' | 'nameSetDeleted'

// What the dictionary popup shows for a key.
export interface Lookup {
  svReading: string
  name: string | null
  // Whether `name` comes from the active nameset rather than the global names.
  nameFromNameSet: boolean
  phrases: string[]
}

export interface DocumentState {
  // Shown in the browser tab; empty for text typed or pasted into the source box.
  title: string
  rawText: string
  pages: string[]
  currentPage: number
  pageLength: number
  nameSetId: number
  document: Paragraph[] | null
}
