import { useCallback, useEffect, useRef, useState } from 'react'
import { BottomBar } from './components/BottomBar'
import { DictPopup } from './components/DictPopup'
import { Header } from './components/Header'
import { NamesetChooser } from './components/NamesetChooser'
import { Workspace } from './components/Workspace'
import { core } from './core/client'
import { paginate } from './core/paginate'
import type { NameSet, Paragraph, TabData, Token } from './core/types'

type CoreStatus =
  | { state: 'loading' }
  | { state: 'ready'; ms: number }
  | { state: 'error'; message: string }

function createInitialTab(): TabData {
  return {
    id: 'tab-' + Date.now(),
    title: 'Untitled',
    rawText: '',
    pages: [],
    currentPage: 0,
    pageLength: 5000,
    nameSetId: -1,
    document: null,
    isEditing: true,
  }
}

export default function App() {
  const [coreStatus, setCoreStatus] = useState<CoreStatus>({ state: 'loading' })
  const [nameSets, setNameSets] = useState<NameSet[]>([])
  const [tabs, setTabs] = useState<TabData[]>([createInitialTab()])
  const [activeTabId, setActiveTabId] = useState<string>(tabs[0].id)

  const [hoveredTokenId, setHoveredTokenId] = useState<number | null>(null)
  const [inspectedToken, setInspectedToken] = useState<Token | null>(null)

  const [isMenuOpen, setIsMenuOpen] = useState(false)
  const [isNamesetChooserOpen, setIsNamesetChooserOpen] = useState(false)
  const [isBusy, setIsBusy] = useState(false)
  const [progressPercent, setProgressPercent] = useState(30)
  const [statusMessage, setStatusMessage] = useState('Loading dictionaries into memory…')

  const fileInputRef = useRef<HTMLInputElement>(null)

  const activeTab = tabs.find((t) => t.id === activeTabId) ?? tabs[0]

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
        setStatusMessage(`Dictionaries ready in ${(ms / 1000).toFixed(1)}s`)
      })
      .catch((error: unknown) => {
        const msg = String(error)
        setCoreStatus({ state: 'error', message: msg })
        setStatusMessage('Error loading core: ' + msg)
      })
  }, [])

  // Conversion runner for a page
  const convertPageText = useCallback(
    async (textToConvert: string, tabId: string, pageIdx: number) => {
      if (!textToConvert.trim()) return

      setIsBusy(true)
      setProgressPercent(40)
      setStatusMessage(`Converting page ${pageIdx + 1}…`)

      try {
        const doc: Paragraph[] = await core.convert(textToConvert)

        setTabs((prevTabs) =>
          prevTabs.map((t) => {
            if (t.id === tabId) {
              return {
                ...t,
                currentPage: pageIdx,
                document: doc,
                isEditing: false,
              }
            }
            return t
          }),
        )

        setProgressPercent(100)
        setStatusMessage('Conversion completed.')
      } catch (err) {
        setStatusMessage(`Conversion error: ${err}`)
      } finally {
        setIsBusy(false)
      }
    },
    [],
  )

  // Load new full text into a tab
  const loadTextIntoTab = useCallback(
    async (text: string, title?: string, fileName?: string) => {
      const pageLen = activeTab.pageLength || 5000
      const pages = paginate(text, pageLen)

      const updatedTab: TabData = {
        ...activeTab,
        title: title || activeTab.title,
        fileName: fileName || activeTab.fileName,
        rawText: text,
        pages,
        currentPage: 0,
        document: null,
        isEditing: false,
      }

      setTabs((prev) => prev.map((t) => (t.id === activeTab.id ? updatedTab : t)))

      if (pages.length > 0) {
        await convertPageText(pages[0], activeTab.id, 0)
      }
    },
    [activeTab, convertPageText],
  )

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
      if (content) {
        if (!activeTab.rawText) {
          await loadTextIntoTab(content, file.name, file.name)
        } else {
          const newTabId = 'tab-' + Date.now()
          const pages = paginate(content, activeTab.pageLength || 5000)
          const newTab: TabData = {
            id: newTabId,
            title: file.name,
            fileName: file.name,
            rawText: content,
            pages,
            currentPage: 0,
            pageLength: activeTab.pageLength || 5000,
            nameSetId: activeTab.nameSetId,
            document: null,
            isEditing: false,
          }
          setTabs((prev) => [...prev, newTab])
          setActiveTabId(newTabId)
          if (pages.length > 0) {
            await convertPageText(pages[0], newTabId, 0)
          }
        }
      }
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
      await loadTextIntoTab(text, 'Clipboard', '')
    } catch {
      setStatusMessage('Clipboard access denied or unsupported.')
    }
  }, [loadTextIntoTab])

  // Save converted text to file
  const handleSaveToFile = useCallback(async () => {
    if (!activeTab.rawText) {
      setStatusMessage('No text to save.')
      return
    }

    setIsBusy(true)
    setStatusMessage('Converting full document to plain text…')
    setProgressPercent(50)

    try {
      const plainVn = await core.convertPlain(activeTab.rawText)
      const blob = new Blob([plainVn], { type: 'text/plain;charset=utf-8' })
      const url = URL.createObjectURL(blob)
      const a = document.createElement('a')
      const baseName = (activeTab.fileName || activeTab.title || 'hanvi').replace(/\.txt$/i, '')
      a.href = url
      a.download = `${baseName}_vietnamese.txt`
      a.click()
      URL.revokeObjectURL(url)

      setProgressPercent(100)
      setStatusMessage('Saved to file.')
    } catch (err) {
      setStatusMessage(`Failed to save: ${err}`)
    } finally {
      setIsBusy(false)
    }
  }, [activeTab])

  // Copy Vietnamese text
  const handleCopyVietnamese = useCallback(() => {
    if (!activeTab.document || activeTab.document.length === 0) {
      setStatusMessage('No converted text to copy.')
      return
    }

    const lines = activeTab.document.map((para) => {
      let line = ''
      for (const tok of para) {
        if (tok.vnSpace) line += ' '
        line += tok.vn
      }
      return line
    })

    const fullText = lines.join('\n')
    void navigator.clipboard.writeText(fullText)
    setStatusMessage('Vietnamese text copied to clipboard.')
  }, [activeTab.document])

  // Re-convert current page
  const handleReconvert = useCallback(async () => {
    if (activeTab.pages.length > 0) {
      await convertPageText(activeTab.pages[activeTab.currentPage], activeTab.id, activeTab.currentPage)
    } else if (activeTab.rawText) {
      await convertPageText(activeTab.rawText, activeTab.id, 0)
    }
  }, [activeTab, convertPageText])

  // Reload dictionaries
  const handleReloadDict = useCallback(async () => {
    setIsBusy(true)
    setStatusMessage('Reloading dictionary…')
    try {
      await core.loadNameSet(activeTab.nameSetId)
      await handleReconvert()
      setStatusMessage('Dictionary reloaded.')
    } catch (err) {
      setStatusMessage(`Reload error: ${err}`)
    } finally {
      setIsBusy(false)
    }
  }, [activeTab.nameSetId, handleReconvert])

  // Tab management
  const handleNewTab = useCallback(() => {
    const newTab = createInitialTab()
    newTab.pageLength = activeTab.pageLength
    newTab.nameSetId = activeTab.nameSetId
    setTabs((prev) => [...prev, newTab])
    setActiveTabId(newTab.id)
  }, [activeTab.pageLength, activeTab.nameSetId])

  const handleCloseTab = useCallback(
    (idToClose: string) => {
      if (tabs.length === 1) {
        setTabs([createInitialTab()])
        return
      }
      const idx = tabs.findIndex((t) => t.id === idToClose)
      const newTabs = tabs.filter((t) => t.id !== idToClose)
      setTabs(newTabs)
      if (activeTabId === idToClose) {
        const nextIdx = Math.max(0, idx - 1)
        setActiveTabId(newTabs[nextIdx].id)
      }
    },
    [tabs, activeTabId],
  )

  // Page navigation
  const handlePageChange = useCallback(
    async (newPage: number) => {
      if (newPage < 0 || newPage >= activeTab.pages.length) return
      await convertPageText(activeTab.pages[newPage], activeTab.id, newPage)
    },
    [activeTab.pages, activeTab.id, convertPageText],
  )

  // Chars per page change
  async function handleCharPerPageChange(newChars: number) {
    if (!activeTab.rawText) {
      setTabs((prev) =>
        prev.map((t) => (t.id === activeTab.id ? { ...t, pageLength: newChars } : t)),
      )
      return
    }
    const newPages = paginate(activeTab.rawText, newChars)
    setTabs((prev) =>
      prev.map((t) =>
        t.id === activeTab.id
          ? {
              ...t,
              pageLength: newChars,
              pages: newPages,
              currentPage: 0,
            }
          : t,
      ),
    )
    if (newPages.length > 0) {
      await convertPageText(newPages[0], activeTab.id, 0)
    }
  }

  // Nameset selection
  async function handleSelectNameset(id: number) {
    await core.loadNameSet(id)
    setTabs((prev) =>
      prev.map((t) => (t.id === activeTab.id ? { ...t, nameSetId: id } : t)),
    )
    await handleReconvert()
  }

  const activeNameset = nameSets.find((s) => s.id === activeTab.nameSetId)
  const activeNamesetTitle = activeNameset ? activeNameset.title : 'None'

  // Ref holding actions for global shortcut listener
  const actionsRef = useRef({
    handleOpenFile,
    handleReadClipboard,
    handleSaveToFile,
    handleCopyVietnamese,
    handleReconvert,
    handleNewTab,
    handleCloseTab,
    handlePageChange,
    activeTab,
    activeTabId,
  })

  useEffect(() => {
    actionsRef.current = {
      handleOpenFile,
      handleReadClipboard,
      handleSaveToFile,
      handleCopyVietnamese,
      handleReconvert,
      handleNewTab,
      handleCloseTab,
      handlePageChange,
      activeTab,
      activeTabId,
    }
  })

  // Global Keyboard Shortcuts
  useEffect(() => {
    function handleKeyDown(e: KeyboardEvent) {
      const activeEl = document.activeElement
      const isInput =
        activeEl instanceof HTMLInputElement || activeEl instanceof HTMLTextAreaElement

      const {
        handleOpenFile: openFile,
        handleReadClipboard: readClip,
        handleSaveToFile: saveFile,
        handleCopyVietnamese: copyVn,
        handleReconvert: reconvert,
        handleNewTab: newTab,
        handleCloseTab: closeTab,
        handlePageChange: pageChange,
        activeTab: curTab,
        activeTabId: curTabId,
      } = actionsRef.current

      // Ctrl + O: Open File
      if (e.ctrlKey && e.key.toLowerCase() === 'o') {
        e.preventDefault()
        openFile()
        return
      }

      // Ctrl + Q: Read Clipboard
      if (e.ctrlKey && e.key.toLowerCase() === 'q') {
        e.preventDefault()
        void readClip()
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

      // Ctrl + R: Re-convert
      if (e.ctrlKey && !e.shiftKey && e.key.toLowerCase() === 'r') {
        e.preventDefault()
        void reconvert()
        return
      }

      // Ctrl + T: New Tab
      if (e.ctrlKey && e.key.toLowerCase() === 't') {
        e.preventDefault()
        newTab()
        return
      }

      // Ctrl + W: Close Tab
      if (e.ctrlKey && e.key.toLowerCase() === 'w') {
        e.preventDefault()
        closeTab(curTabId)
        return
      }

      // Arrow Left/Right: Page navigation
      if (!isInput && curTab.pages.length > 1) {
        if (e.key === 'ArrowLeft' && curTab.currentPage > 0) {
          e.preventDefault()
          void pageChange(curTab.currentPage - 1)
        } else if (e.key === 'ArrowRight' && curTab.currentPage < curTab.pages.length - 1) {
          e.preventDefault()
          void pageChange(curTab.currentPage + 1)
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
        tabs={tabs}
        activeTabId={activeTabId}
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
        onSelectTab={setActiveTabId}
        onNewTab={handleNewTab}
        onCloseTab={handleCloseTab}
        onOpenFile={handleOpenFile}
        onReadClipboard={handleReadClipboard}
        onSaveToFile={handleSaveToFile}
        onCopyVietnamese={handleCopyVietnamese}
        onOpenNamesetChooser={() => setIsNamesetChooserOpen(true)}
        onReconvert={handleReconvert}
        onReloadDict={handleReloadDict}
      />

      <Workspace
        tab={activeTab}
        hoveredTokenId={hoveredTokenId}
        activeTokenId={inspectedToken ? inspectedToken.id : null}
        isBusy={isBusy}
        onHoverToken={setHoveredTokenId}
        onClickToken={setInspectedToken}
        onConvertText={(text) => loadTextIntoTab(text)}
        onPageChange={handlePageChange}
        onToggleEdit={() => {
          setTabs((prev) =>
            prev.map((t) => (t.id === activeTab.id ? { ...t, isEditing: !t.isEditing } : t)),
          )
        }}
        onCopyVietnamese={handleCopyVietnamese}
      />

      <BottomBar
        progressPercent={progressPercent}
        statusMessage={statusMessage}
        activeNamesetTitle={activeNamesetTitle}
        charPerPage={activeTab.pageLength}
        onOpenNamesetChooser={() => setIsNamesetChooserOpen(true)}
        onCharPerPageChange={handleCharPerPageChange}
      />

      <DictPopup token={inspectedToken} onClose={() => setInspectedToken(null)} />

      <NamesetChooser
        isOpen={isNamesetChooserOpen}
        nameSets={nameSets}
        activeSetId={activeTab.nameSetId}
        onSelect={handleSelectNameset}
        onClose={() => setIsNamesetChooserOpen(false)}
      />
    </div>
  )
}
