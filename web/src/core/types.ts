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
}

export type Paragraph = Token[]

export interface NameSet {
  id: number
  title: string
}

export interface TabData {
  id: string
  title: string
  fileName?: string
  rawText: string
  pages: string[]
  currentPage: number
  pageLength: number
  nameSetId: number
  document: Paragraph[] | null
  isEditing: boolean
}
