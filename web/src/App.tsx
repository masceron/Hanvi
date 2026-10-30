import * as Comlink from 'comlink'
import { useCallback, useEffect, useMemo, useRef, useState } from 'react'
import { BottomBar } from './components/BottomBar'
import { DictPopup } from './components/DictPopup'
import type { EditTarget } from './components/editTarget'
import { EntriesEditor, type EntriesSource } from './components/EntriesEditor'
import { Header } from './components/Header'
import { NamesetChooser } from './components/NamesetChooser'
import { NamesetsManager } from './components/NamesetsManager'
import { RulePopup } from './components/RulePopup'
import { Workspace } from './components/Workspace'
import { core } from './core/client'
import { paginate } from './core/paginate'
import type { DocumentState, NameSet, Rule, Token } from './core/types'
import { download } from './download'

type CoreStatus =
  | { state: 'loading' }
  | { state: 'ready'; ms: number }
  | { state: 'error'; message: string }

type Dialog =
  | { kind: 'dict'; text: string }
  | { kind: 'rule'; rule: Rule }
  | { kind: 'entries'; source: EntriesSource }
  | { kind: 'namesets' }

const INITIAL_DOCUMENT: DocumentState = {
  title: '',
  rawText: '',
  pages: [],
  currentPage: 0,
  pageLength: 5000,
  nameSetId: -1,
  document: null,
}

export default function App() {
  const [coreStatus, setCoreStatus] = useState<CoreStatus>({ state: 'loading' })
  const [nameSets, setNameSets] = useState<NameSet[]>([])
  const [doc, setDoc] = useState<DocumentState>(INITIAL_DOCUMENT)

  const [activeTokenId, setActiveTokenId] = useState<number | null>(null)
  const [dialog, setDialog] = useState<Dialog | null>(null)

  const [isMenuOpen, setIsMenuOpen] = useState(false)
  const [isNamesetChooserOpen, setIsNamesetChooserOpen] = useState(false)
  const [isBusy, setIsBusy] = useState(false)
  const [progressPercent, setProgressPercent] = useState(30)
  const [statusMessage, setStatusMessage] = useState('Loading dictionaries into memory…')

  const fileInputRef = useRef<HTMLInputElement>(null)
  // Only the latest conversion request may update the document.
  const conversionId = useRef(0)

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
    const text = pages[pageIdx] ?? ''
    if (!text.trim()) {
      setDoc((d) => ({ ...d, currentPage: 0, document: null }))
      return
    }

    setIsBusy(true)
    setProgressPercent(40)
    setStatusMessage(`Converting page ${pageIdx + 1}…`)

    try {
      const converted = await core.convert(text)
      if (id !== conversionId.current) return
      setDoc((d) => ({ ...d, currentPage: pageIdx, document: converted }))
      setProgressPercent(100)
      setStatusMessage('Conversion completed.')
    } catch (err) {
      setStatusMessage(`Conversion error: ${err}`)
    } finally {
      if (id === conversionId.current) setIsBusy(false)
    }
  }, [])

  // Replaces the whole text (file, clipboard) and shows its first page. The Chinese pane is not
  // editable, as on the desktop: text only comes in whole, which keeps all panes token-aligned.
  const loadText = useCallback(
    async (text: string, title: string) => {
      const pages = paginate(text, doc.pageLength)
      setActiveTokenId(null)
      setDoc((d) => ({ ...d, title, rawText: text, pages, currentPage: 0, document: null }))
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

  // Clipboard handler
  const handleReadClipboard = useCallback(async () => {
    try {
      const text = await navigator.clipboard.readText()
      if (!text.trim()) {
        setStatusMessage('Clipboard is empty.')
        return
      }
      await loadText(text, '')
    } catch {
      setStatusMessage('Clipboard access denied or unsupported.')
    }
  }, [loadText])

  // Save converted text to file
  const handleSaveToFile = useCallback(async () => {
    if (!doc.rawText) {
      setStatusMessage('No text to save.')
      return
    }

    setIsBusy(true)
    setStatusMessage('Converting full document to plain text…')
    setProgressPercent(50)

    try {
      const plainVn = await core.convertPlain(doc.rawText)
      const baseName = (doc.title || 'hanvi').replace(/\.txt$/i, '')
      download(`${baseName}_vietnamese.txt`, plainVn)

      setProgressPercent(100)
      setStatusMessage('Saved to file.')
    } catch (err) {
      setStatusMessage(`Failed to save: ${err}`)
    } finally {
      setIsBusy(false)
    }
  }, [doc.rawText, doc.title])

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

    void navigator.clipboard.writeText(lines.join('\n'))
    setStatusMessage('Vietnamese text copied to clipboard.')
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

  const requestedPersistence = useRef(false)
  function dialogClosed(changed: boolean) {
    setDialog(null)
    if (!changed) return
    // Ask once that the browser keep the stored dictionary under storage pressure.
    if (!requestedPersistence.current) {
      requestedPersistence.current = true
      void navigator.storage?.persist?.()
    }
    void handleDictionaryChanged()
  }

  const handleEditRequest = useCallback((target: EditTarget) => {
    setDialog(target.kind === 'rule' ? { kind: 'rule', rule: target.rule } : { kind: 'dict', text: target.text })
  }, [])

  const handleClickToken = useCallback((token: Token | null) => setActiveTokenId(token?.id ?? null), [])

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
    const pages = paginate(doc.rawText, newChars)
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
    }
  })

  // Another tab saved dictionary edits; the worker has loaded them.
  useEffect(() => {
    void core.onExternalChange(
      Comlink.proxy(() => {
        setStatusMessage('Dictionary updated from another tab.')
        void actionsRef.current.handleDictionaryChanged()
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
          if (next !== undefined) setActiveTokenId(next)
          return
        }
        if (e.key === 'Enter' || e.key === ' ' || e.key.toLowerCase() === 'e') {
          e.preventDefault()
          const token = byId.get(activeId)!
          if (token.rule) editRequest({ kind: 'rule', rule: token.rule })
          else if (token.cn) editRequest({ kind: 'text', text: token.cn })
          return
        }
        if (e.key === 'Escape') {
          setActiveTokenId(null)
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
        onReconvert={handleReconvert}
        onReloadDict={handleReloadDict}
      />

      <Workspace
        doc={doc}
        activeTokenId={activeTokenId}
        onClickToken={handleClickToken}
        onEditRequest={handleEditRequest}
        onPageChange={handlePageChange}
        onCopyVietnamese={handleCopyVietnamese}
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
        <DictPopup text={dialog.text} activeNameSet={activeNameset} onClose={dialogClosed} />
      )}
      {dialog?.kind === 'rule' && <RulePopup rule={dialog.rule} onClose={() => setDialog(null)} />}
      {dialog?.kind === 'entries' && (
        <EntriesEditor source={dialog.source} activeNameSet={activeNameset} onClose={dialogClosed} />
      )}
      {dialog?.kind === 'namesets' && <NamesetsManager activeNameSet={activeNameset} onClose={dialogClosed} />}

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
