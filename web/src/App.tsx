import * as Comlink from 'comlink'
import { useCallback, useEffect, useMemo, useRef, useState } from 'react'
import { useRegisterSW } from 'virtual:pwa-register/react'
import { BottomBar } from './components/BottomBar'
import { ConfirmDialog } from './components/ConfirmDialog'
import { DictPopup } from './components/DictPopup'
import { editTargetOfTokens, type EditTarget, type Role } from './components/editTarget'
import { EntriesEditor, type EntriesSource } from './components/EntriesEditor'
import { HistoryDialog } from './components/HistoryDialog'
import { Header } from './components/Header'
import { NamesetChooser } from './components/NamesetChooser'
import { NamesetsManager } from './components/NamesetsManager'
import { PasteDialog } from './components/PasteDialog'
import { RulePopup } from './components/RulePopup'
import { Workspace } from './components/Workspace'
import { convert, core } from './core/client'
import { paginate } from './core/paginate'
import type { DocumentState, NameSet, Rule, Token, TokenOverride } from './core/types'
import { readClipboard, writeClipboard } from './clipboard'
import { download } from './download'
import { useIsMobileBrowser, type FlatToken, type TokenSelection } from './mobile'
import { describeStorage, requestPersistence } from './storage'

type CoreStatus =
  | { state: 'loading' }
  | { state: 'ready'; ms: number }
  | { state: 'error'; message: string }

type Dialog =
  | { kind: 'dict'; text: string; token?: Token; picked?: string | null }
  | { kind: 'rule'; rule: Rule }
  | { kind: 'entries'; source: EntriesSource }
  | { kind: 'namesets' }
  | { kind: 'history' }
  | { kind: 'import'; file: File }
  | { kind: 'reset' }

const INITIAL_DOCUMENT: DocumentState = {
  title: '',
  text: '',
  pages: [],
  currentPage: 0,
  pageLength: 5000,
  nameSetId: -1,
  document: null,
}

export default function App() {
  const {
    needRefresh: [needRefresh, setNeedRefresh],
    updateServiceWorker,
  } = useRegisterSW({
    onRegisterError(error) {
      console.error('Service worker registration failed:', error)
    },
  })
  const isMobile = useIsMobileBrowser()
  const [coreStatus, setCoreStatus] = useState<CoreStatus>({ state: 'loading' })
  const [nameSets, setNameSets] = useState<NameSet[]>([])
  const [doc, setDoc] = useState<DocumentState>(INITIAL_DOCUMENT)

  const [activeTokenId, setActiveTokenId] = useState<number | null>(null)
  const [selection, setSelection] = useState<TokenSelection | null>(null)
  const [dialog, setDialog] = useState<Dialog | null>(null)
  const [isPasteDialogOpen, setIsPasteDialogOpen] = useState(false)

  // Flattened tokens of the current page in reading order
  const flatTokens = useMemo<FlatToken[]>(() => {
    if (!doc.document) return []
    const result: FlatToken[] = []
    for (let p = 0; p < doc.document.length; p++) {
      const para = doc.document[p]
      for (let t = 0; t < para.length; t++) {
        result.push({ pIdx: p, tIdx: t, token: para[t] })
      }
    }
    return result
  }, [doc.document])

  const selectedTokenIds = useMemo<number[]>(() => {
    if (!selection) return []
    const selected = flatTokens.slice(selection.startIndex, selection.endIndex + 1)
    return [...new Set(selected.map((ft) => ft.token.id))]
  }, [selection, flatTokens])

  const [isMenuOpen, setIsMenuOpen] = useState(false)
  const [isNamesetChooserOpen, setIsNamesetChooserOpen] = useState(false)
  const [isBusy, setIsBusy] = useState(false)
  const [progressPercent, setProgressPercent] = useState(30)
  const [statusMessage, setStatusMessage] = useState('Loading dictionaries into memory…')

  const [storageNote, setStorageNote] = useState('')

  const fileInputRef = useRef<HTMLInputElement>(null)
  const dbInputRef = useRef<HTMLInputElement>(null)
  // Only the latest conversion request may update the document.
  const conversionId = useRef(0)
  // Translations picked for single occurrences in the loaded text, kept until another text is
  // loaded. Every conversion applies them; one whose word a dictionary edit re-segmented is dropped.
  const overrides = useRef<TokenOverride[]>([])

  useEffect(() => {
    document.title = doc.title ? `${doc.title} – Hanvi` : 'Hanvi'
  }, [doc.title])

  // Initialize Core and WASM
  useEffect(() => {
    const started = performance.now()

    core
      .init()
      .then(async () => {
        const sets = await core.nameSets()
        setNameSets(sets)
        const ms = performance.now() - started
        setCoreStatus({ state: 'ready', ms })
        setProgressPercent(100)
        setStatusMessage(
          (await core.hasStorage())
            ? `Dictionaries ready in ${(ms / 1000).toFixed(1)}s`
            : 'Dictionaries ready, but this browser cannot store them: edits last until the tab is closed.',
        )
      })
      .catch((error: unknown) => {
        const msg = String(error)
        setCoreStatus({ state: 'error', message: msg })
        setStatusMessage('Error loading core: ' + msg)
      })
  }, [])

  // Converts one page and shows it, unless a newer request has started meanwhile.
  const convertPage = useCallback(async (pages: string[], pageIdx: number) => {
    const id = ++conversionId.current
    // The selection is a range of the page's tokens, which a conversion replaces.
    setSelection(null)
    const text = pages[pageIdx] ?? ''
    if (!text.trim()) {
      setDoc((d) => ({ ...d, currentPage: 0, document: null }))
      return
    }

    setIsBusy(true)
    setProgressPercent(40)
    setStatusMessage(`Converting page ${pageIdx + 1}…`)

    try {
      const pageStart = pages.slice(0, pageIdx).reduce((sum, page) => sum + page.length, 0)
      const { paragraphs, stale } = await convert(text, overrides.current, pageStart)
      if (id !== conversionId.current) return
      setDoc((d) => ({ ...d, currentPage: pageIdx, document: paragraphs }))
      setProgressPercent(100)
      if (stale.length > 0) {
        overrides.current = overrides.current.filter((o) => !stale.includes(o))
        setStatusMessage(
          stale.length === 1
            ? 'Conversion completed. A translation picked for one occurrence no longer fits the text and was dropped.'
            : `Conversion completed. ${stale.length} translations picked for single occurrences no longer fit the text and were dropped.`,
        )
      } else {
        setStatusMessage('Conversion completed.')
      }
    } catch (err) {
      setStatusMessage(`Conversion error: ${err}`)
    } finally {
      if (id === conversionId.current) setIsBusy(false)
    }
  }, [])

  // Replaces the whole text (file, clipboard) and shows its first page. The Chinese pane is not
  // editable, as on the desktop: text only comes in whole, which keeps all panes token-aligned.
  // The text is normalized here, once, so that offsets into it (tokens', overrides') stay valid.
  const loadText = useCallback(
    async (raw: string, title: string) => {
      const text = await core.normalize(raw)
      const pages = paginate(text, doc.pageLength)
      overrides.current = []
      setActiveTokenId(null)
      setDoc((d) => ({ ...d, title, text, pages, currentPage: 0, document: null }))
      await convertPage(pages, 0)
    },
    [doc.pageLength, convertPage],
  )

  // Ctrl+V anywhere but a text field reads the pasted text, like the desktop's Paste shortcut.
  useEffect(() => {
    function handlePaste(e: ClipboardEvent) {
      const target = e.target as HTMLElement | null
      if (target?.closest('input, textarea, .modal-backdrop')) return
      const text = e.clipboardData?.getData('text/plain') ?? ''
      if (!text.trim()) return
      e.preventDefault()
      void loadText(text, '')
    }
    window.addEventListener('paste', handlePaste)
    return () => window.removeEventListener('paste', handlePaste)
  }, [loadText])

  // File picker handler
  const handleOpenFile = useCallback(() => {
    fileInputRef.current?.click()
  }, [])

  function handleFileSelected(e: React.ChangeEvent<HTMLInputElement>) {
    const file = e.target.files?.[0]
    if (!file) return

    const reader = new FileReader()
    reader.onload = async (evt) => {
      const content = evt.target?.result as string
      if (content) await loadText(content, file.name)
    }
    reader.readAsText(file)
    e.target.value = ''
  }

  // Reads the clipboard, or opens a paste box where the page may not (phones need a way to
  // paste that does not rely on Ctrl+V).
  const handleReadClipboard = useCallback(async () => {
    const text = await readClipboard()
    if (text === null) {
      setIsPasteDialogOpen(true)
      return
    }
    if (!text.trim()) {
      setStatusMessage('Clipboard is empty.')
      return
    }
    await loadText(text, '')
  }, [loadText])

  // Save converted text to file
  const handleSaveToFile = useCallback(async () => {
    if (!doc.text) {
      setStatusMessage('No text to save.')
      return
    }

    setIsBusy(true)
    setStatusMessage('Converting full document to plain text…')
    setProgressPercent(50)

    try {
      const plainVn = await core.convertPlain(doc.text, overrides.current)
      const baseName = (doc.title || 'hanvi').replace(/\.txt$/i, '')
      download(`${baseName}_vietnamese.txt`, plainVn)

      setProgressPercent(100)
      setStatusMessage('Saved to file.')
    } catch (err) {
      setStatusMessage(`Failed to save: ${err}`)
    } finally {
      setIsBusy(false)
    }
  }, [doc.text, doc.title])

  // Copy Vietnamese text
  const handleCopyVietnamese = useCallback(() => {
    if (!doc.document || doc.document.length === 0) {
      setStatusMessage('No converted text to copy.')
      return
    }

    const lines = doc.document.map((para) => {
      let line = ''
      for (const tok of para) {
        if (tok.vnSpace) line += ' '
        line += tok.vn
      }
      return line
    })

    void writeClipboard(lines.join('\n')).then((copied) =>
      setStatusMessage(copied ? 'Vietnamese text copied to clipboard.' : 'Could not copy to the clipboard.'),
    )
  }, [doc.document])

  // Re-convert current page
  const handleReconvert = useCallback(async () => {
    await convertPage(doc.pages, doc.currentPage)
  }, [doc.pages, doc.currentPage, convertPage])

  // After the dictionary changed (here or in another tab): refresh namesets, follow the core if
  // it dropped a deleted active nameset, and re-convert the page.
  const handleDictionaryChanged = useCallback(async () => {
    const [sets, active] = await Promise.all([core.nameSets(), core.activeNameSet()])
    setNameSets(sets)
    setDoc((d) => ({ ...d, nameSetId: active }))
    await handleReconvert()
  }, [handleReconvert])

  // Reload dictionaries
  const handleReloadDict = useCallback(async () => {
    setIsBusy(true)
    setStatusMessage('Reloading dictionary…')
    try {
      await core.reloadDict()
      await handleDictionaryChanged()
      setStatusMessage('Dictionary reloaded.')
    } catch (err) {
      setStatusMessage(`Reload error: ${err}`)
    } finally {
      setIsBusy(false)
    }
  }, [handleDictionaryChanged])

  function dialogClosed(changed: boolean) {
    setDialog(null)
    if (!changed) return
    void requestPersistence()
    void handleDictionaryChanged()
  }

  // The menu says where the dictionary is kept; it may have changed since it last opened.
  useEffect(() => {
    if (isMenuOpen && coreStatus.state === 'ready') void describeStorage().then(setStorageNote)
  }, [isMenuOpen, coreStatus.state])

  // Downloads the whole dictionary as a dict.db the desktop app can open.
  const handleExportDict = useCallback(async () => {
    setIsBusy(true)
    setStatusMessage('Exporting the dictionary…')
    try {
      download('dict.db', await core.exportDb())
      setStatusMessage('Dictionary exported as dict.db.')
    } catch (err) {
      setStatusMessage(`Export failed: ${err}`)
    } finally {
      setIsBusy(false)
    }
  }, [])

  function handleDbFileSelected(e: React.ChangeEvent<HTMLInputElement>) {
    const file = e.target.files?.[0]
    e.target.value = ''
    if (file) setDialog({ kind: 'import', file })
  }

  // The page is re-converted even after a failure, which may come after the swap (when storing
  // it); the outcome is shown last, as re-converting reports its own status.
  async function replaceDictionary(working: string, done: string, replace: () => Promise<void>) {
    setIsBusy(true)
    setStatusMessage(working)
    let outcome = done
    try {
      await replace()
    } catch (err) {
      outcome = err instanceof Error ? err.message : String(err)
    }
    await handleDictionaryChanged()
    setIsBusy(false)
    setStatusMessage(outcome)
  }

  function handleImportDict(file: File) {
    void replaceDictionary(`Importing ${file.name}…`, `Imported ${file.name}.`, async () => {
      const bytes = new Uint8Array(await file.arrayBuffer())
      await core.importDb(Comlink.transfer(bytes, [bytes.buffer]))
      void requestPersistence()
    })
  }

  function handleResetDict() {
    void replaceDictionary('Restoring the bundled dictionary…', 'Restored the bundled dictionary.', () =>
      core.resetDb(),
    )
  }

  const handleEditRequest = useCallback((target: EditTarget) => {
    setDialog(
      target.kind === 'rule'
        ? { kind: 'rule', rule: target.rule }
        : {
            kind: 'dict',
            text: target.text,
            token: target.token,
            // As picked, not as shown: the token's vn may carry a sentence's capital.
            picked: target.token && (overrides.current.find((o) => o.offset === target.token!.offset)?.vn ?? null),
          },
    )
  }, [])

  // Picks a translation for one occurrence of a word, or (null) goes back to the dictionary's. The
  // popup reports a change when it closes, which re-converts the page.
  const handleOverride = useCallback((token: Token, vn: string | null) => {
    const rest = overrides.current.filter((o) => o.offset !== token.offset)
    overrides.current = vn === null ? rest : [...rest, { offset: token.offset, cn: token.cn, vn }]
  }, [])

  // Touch screens have no right-click (iOS fires no contextmenu at all), so tapping inside the
  // selection edits it. A ref keeps the callbacks below, and so the panes, from changing on
  // every click.
  const selectionRef = useRef<TokenSelection | null>(null)
  useEffect(() => {
    selectionRef.current = selection
  }, [selection])

  // Edits the selected tokens; false when there is nothing to edit.
  const editSelection = useCallback(() => {
    const cur = selectionRef.current
    if (!cur) return false
    const tokens = flatTokens.slice(cur.startIndex, cur.endIndex + 1).map((ft) => ft.token)
    const target = editTargetOfTokens(tokens)
    if (target) handleEditRequest(target)
    return target !== null
  }, [flatTokens, handleEditRequest])

  const handleClickToken = useCallback(
    (
      token: Token | null,
      viaTouch: boolean,
      pIdx?: number,
      tIdx?: number,
      role: Role = 'vn',
    ) => {
      if (!token || pIdx === undefined || tIdx === undefined) {
        setActiveTokenId(null)
        setSelection(null)
        return
      }

      const flatIndex = flatTokens.findIndex((ft) => ft.pIdx === pIdx && ft.tIdx === tIdx)
      if (flatIndex === -1) {
        setActiveTokenId(token.id)
        setSelection(null)
        return
      }

      const curSel = selectionRef.current
      if (viaTouch && curSel && flatIndex >= curSel.startIndex && flatIndex <= curSel.endIndex) {
        editSelection()
        return
      }

      setActiveTokenId(token.id)
      setSelection({
        startIndex: flatIndex,
        endIndex: flatIndex,
        anchorIndex: flatIndex,
        activeRole: role,
      })

      // Only phones get the toolbar; other touch screens (a touch laptop) edit with a second tap.
      if (isMobile) setStatusMessage('Tap ◀ / ▶ to expand selection, or tap the word to edit.')
      else if (viaTouch) setStatusMessage('Tap the highlighted word again to edit it.')
    },
    [flatTokens, editSelection, isMobile],
  )

  // Grows the selection by one token to the left (-1) or right (1).
  const handleExpand = useCallback(
    (step: -1 | 1) => {
      const cur = selectionRef.current
      if (!cur) return
      const next =
        step < 0 ? { ...cur, startIndex: cur.startIndex - 1 } : { ...cur, endIndex: cur.endIndex + 1 }
      if (next.startIndex < 0 || next.endIndex >= flatTokens.length) return
      // Set here too, so a second tap before the next render grows it again.
      selectionRef.current = next
      setSelection(next)
      setStatusMessage(`Selected ${next.endIndex - next.startIndex + 1} words. Tap Edit to edit.`)
    },
    [flatTokens.length],
  )

  const handleClearSelection = useCallback(() => {
    setSelection(null)
    setActiveTokenId(null)
  }, [])

  // Page navigation
  const handlePageChange = useCallback(
    async (newPage: number) => {
      if (newPage < 0 || newPage >= doc.pages.length) return
      setActiveTokenId(null) // token ids restart on every page
      await convertPage(doc.pages, newPage)
    },
    [doc.pages, convertPage],
  )

  // Chars per page change
  async function handleCharPerPageChange(newChars: number) {
    const pages = paginate(doc.text, newChars)
    setActiveTokenId(null)
    setDoc((d) => ({ ...d, pageLength: newChars, pages, currentPage: 0 }))
    await convertPage(pages, 0)
  }

  // Nameset selection
  async function handleSelectNameset(id: number) {
    await core.loadNameSet(id)
    setDoc((d) => ({ ...d, nameSetId: id }))
    await handleReconvert()
  }

  const activeNameset = nameSets.find((s) => s.id === doc.nameSetId) ?? null
  const activeNamesetTitle = activeNameset ? activeNameset.title : 'None'

  function openActiveNamesetEntries() {
    if (activeNameset) setDialog({ kind: 'entries', source: { kind: 'nameset', nameSet: activeNameset } })
    else setStatusMessage('No nameset is active. Choose one first.')
  }

  // Tokens of the page by id, in reading order, for keyboard navigation. A grammar rule's start
  // and end tokens share one id.
  const tokensById = useMemo(() => {
    const byId = new Map<number, Token>()
    for (const paragraph of doc.document ?? []) {
      for (const token of paragraph) if (!byId.has(token.id)) byId.set(token.id, token)
    }
    return byId
  }, [doc.document])
  const tokenIds = useMemo(() => [...tokensById.keys()], [tokensById])

  // Ref holding actions for global shortcut listener
  const actionsRef = useRef({
    handleOpenFile,
    handleReadClipboard,
    handleSaveToFile,
    handleCopyVietnamese,
    handleReconvert,
    handlePageChange,
    handleDictionaryChanged,
    handleEditRequest,
    doc,
    activeTokenId,
    tokensById,
    tokenIds,
    flatTokens,
    editSelection,
  })

  useEffect(() => {
    actionsRef.current = {
      handleOpenFile,
      handleReadClipboard,
      handleSaveToFile,
      handleCopyVietnamese,
      handleReconvert,
      handlePageChange,
      handleDictionaryChanged,
      handleEditRequest,
      doc,
      activeTokenId,
      tokensById,
      tokenIds,
      flatTokens,
      editSelection,
    }
  })

  // Another tab saved dictionary edits; the worker has loaded them.
  useEffect(() => {
    void core.onExternalChange(
      Comlink.proxy(async () => {
        await actionsRef.current.handleDictionaryChanged()
        setStatusMessage('Dictionary updated from another tab.')
      }),
    )
    void core.onSaveError(
      Comlink.proxy((message: string) => setStatusMessage(`Could not save the dictionary: ${message}`)),
    )
  }, [])

  // Global Keyboard Shortcuts
  useEffect(() => {
    function handleKeyDown(e: KeyboardEvent) {
      const activeEl = document.activeElement
      const isInput =
        activeEl instanceof HTMLInputElement || activeEl instanceof HTMLTextAreaElement
      // Dialogs handle their own keys.
      if (document.querySelector('.modal-backdrop')) return

      const {
        handleOpenFile: openFile,
        handleSaveToFile: saveFile,
        handleCopyVietnamese: copyVn,
        handleReconvert: reconvert,
        handlePageChange: pageChange,
        handleEditRequest: editRequest,
        doc: curDoc,
        activeTokenId: activeId,
        tokensById: byId,
        tokenIds: ids,
        flatTokens: flat,
        editSelection: editSelected,
      } = actionsRef.current

      // Ctrl + O: Open File
      if (e.ctrlKey && e.key.toLowerCase() === 'o') {
        e.preventDefault()
        openFile()
        return
      }

      // Ctrl + S: Save File
      if (e.ctrlKey && e.key.toLowerCase() === 's') {
        e.preventDefault()
        void saveFile()
        return
      }

      // Ctrl + Shift + C: Copy Vietnamese
      if (e.ctrlKey && e.shiftKey && e.key.toLowerCase() === 'c') {
        e.preventDefault()
        copyVn()
        return
      }

      // Ctrl + D / Ctrl + Shift + D: phrase / global name lists
      if (e.ctrlKey && e.key.toLowerCase() === 'd') {
        e.preventDefault()
        setDialog({ kind: 'entries', source: { kind: e.shiftKey ? 'names' : 'phrases' } })
        return
      }

      // Ctrl + H: recent dictionary changes
      if (e.ctrlKey && !e.shiftKey && e.key.toLowerCase() === 'h') {
        e.preventDefault()
        setDialog({ kind: 'history' })
        return
      }

      // Ctrl + R: Re-convert
      if (e.ctrlKey && !e.shiftKey && e.key.toLowerCase() === 'r') {
        e.preventDefault()
        void reconvert()
        return
      }

      if (isInput || e.ctrlKey || e.altKey || e.metaKey) return

      // With a token highlighted, as on the desktop: Left/Right move to the previous/next token
      // (the panes scroll to it), Enter/Space/E edit it, Escape clears the highlight.
      if (activeId !== null && byId.has(activeId)) {
        const index = ids.indexOf(activeId)
        if (e.key === 'ArrowLeft' || e.key === 'ArrowRight') {
          e.preventDefault()
          const next = ids[index + (e.key === 'ArrowLeft' ? -1 : 1)]
          if (next !== undefined) {
            setActiveTokenId(next)
            const nextIdx = flat.findIndex((ft) => ft.token.id === next)
            setSelection(
              nextIdx === -1
                ? null
                : { startIndex: nextIdx, endIndex: nextIdx, anchorIndex: nextIdx, activeRole: 'vn' },
            )
          }
          return
        }
        if (e.key === 'Enter' || e.key === ' ' || e.key.toLowerCase() === 'e') {
          e.preventDefault()
          // A re-convert clears the selection but keeps the highlighted token.
          if (!editSelected()) {
            const target = editTargetOfTokens([byId.get(activeId)!])
            if (target) editRequest(target)
          }
          return
        }
        if (e.key === 'Escape') {
          setActiveTokenId(null)
          setSelection(null)
          return
        }
      }

      // Arrow Left/Right: Page navigation
      if (curDoc.pages.length > 1) {
        if (e.key === 'ArrowLeft' && curDoc.currentPage > 0) {
          e.preventDefault()
          void pageChange(curDoc.currentPage - 1)
        } else if (e.key === 'ArrowRight' && curDoc.currentPage < curDoc.pages.length - 1) {
          e.preventDefault()
          void pageChange(curDoc.currentPage + 1)
        }
      }
    }

    window.addEventListener('keydown', handleKeyDown)
    return () => window.removeEventListener('keydown', handleKeyDown)
  }, [])

  return (
    <div className="hanvi-app">
      <input
        ref={fileInputRef}
        type="file"
        accept=".txt,text/plain"
        style={{ display: 'none' }}
        onChange={handleFileSelected}
      />
      {/* No accept filter: iOS greys out files whose type it does not know, and .db is one.
          The worker checks the file instead. */}
      <input ref={dbInputRef} type="file" style={{ display: 'none' }} onChange={handleDbFileSelected} />

      <Header
        isMenuOpen={isMenuOpen}
        statusText={
          coreStatus.state === 'loading'
            ? 'Loading…'
            : coreStatus.state === 'error'
              ? 'Core Error'
              : 'Ready'
        }
        isBusy={isBusy}
        hasUpdate={needRefresh}
        onApplyUpdate={() => void updateServiceWorker(true)}
        onDismissUpdate={() => setNeedRefresh(false)}
        onToggleMenu={() => setIsMenuOpen((prev) => !prev)}
        onCloseMenu={() => setIsMenuOpen(false)}
        onOpenFile={handleOpenFile}
        onReadClipboard={handleReadClipboard}
        onSaveToFile={handleSaveToFile}
        onCopyVietnamese={handleCopyVietnamese}
        onOpenNamesetChooser={() => setIsNamesetChooserOpen(true)}
        onOpenNamesetEntries={openActiveNamesetEntries}
        onOpenNamesetsManager={() => setDialog({ kind: 'namesets' })}
        onOpenPhrases={() => setDialog({ kind: 'entries', source: { kind: 'phrases' } })}
        onOpenNames={() => setDialog({ kind: 'entries', source: { kind: 'names' } })}
        onOpenHistory={() => setDialog({ kind: 'history' })}
        onReconvert={handleReconvert}
        onReloadDict={handleReloadDict}
        onExportDict={handleExportDict}
        onImportDict={() => dbInputRef.current?.click()}
        onResetDict={() => setDialog({ kind: 'reset' })}
        storageNote={storageNote}
      />

      <Workspace
        doc={doc}
        activeTokenId={activeTokenId}
        selectedTokenIds={selectedTokenIds}
        selection={dialog ? null : selection}
        flatTokens={flatTokens}
        isMobile={isMobile}
        onExpand={handleExpand}
        onEditSelection={editSelection}
        onClearSelection={handleClearSelection}
        onClickToken={handleClickToken}
        onEditRequest={handleEditRequest}
        onPageChange={handlePageChange}
        onCopyVietnamese={handleCopyVietnamese}
        onPaste={handleReadClipboard}
        onOpenFile={handleOpenFile}
      />

      <BottomBar
        progressPercent={progressPercent}
        statusMessage={statusMessage}
        activeNamesetTitle={activeNamesetTitle}
        charPerPage={doc.pageLength}
        onOpenNamesetChooser={() => setIsNamesetChooserOpen(true)}
        onCharPerPageChange={handleCharPerPageChange}
      />

      {dialog?.kind === 'dict' && (
        <DictPopup
          text={dialog.text}
          token={dialog.token}
          picked={dialog.picked ?? null}
          activeNameSet={activeNameset}
          onOverride={handleOverride}
          onClose={dialogClosed}
        />
      )}
      {dialog?.kind === 'rule' && <RulePopup rule={dialog.rule} onClose={() => setDialog(null)} />}
      {dialog?.kind === 'entries' && (
        <EntriesEditor source={dialog.source} activeNameSet={activeNameset} onClose={dialogClosed} />
      )}
      {dialog?.kind === 'namesets' && <NamesetsManager activeNameSet={activeNameset} onClose={dialogClosed} />}
      {dialog?.kind === 'history' && <HistoryDialog nameSets={nameSets} onClose={dialogClosed} />}
      {dialog?.kind === 'import' && (
        <ConfirmDialog
          title="Import dictionary"
          confirmLabel="Replace"
          extraAction={{ label: 'Export current first', onClick: () => void handleExportDict() }}
          onConfirm={() => handleImportDict(dialog.file)}
          onClose={() => setDialog(null)}
        >
          <p>
            Replace your dictionary with <strong>{dialog.file.name}</strong>? All of your current phrases, names
            and namesets, edits included, are replaced; other open tabs switch too.
          </p>
        </ConfirmDialog>
      )}
      {dialog?.kind === 'reset' && (
        <ConfirmDialog
          title="Reset dictionary"
          confirmLabel="Reset"
          extraAction={{ label: 'Export current first', onClick: () => void handleExportDict() }}
          onConfirm={handleResetDict}
          onClose={() => setDialog(null)}
        >
          <p>
            Go back to the dictionary that comes with Hanvi? Every edit and nameset you made, and any imported
            dictionary, is discarded.
          </p>
        </ConfirmDialog>
      )}

      {isPasteDialogOpen && (
        <PasteDialog onText={(text) => void loadText(text, '')} onClose={() => setIsPasteDialogOpen(false)} />
      )}

      <NamesetChooser
        isOpen={isNamesetChooserOpen}
        nameSets={nameSets}
        activeSetId={doc.nameSetId}
        onSelect={handleSelectNameset}
        onClose={() => setIsNamesetChooserOpen(false)}
      />
    </div>
  )
}
