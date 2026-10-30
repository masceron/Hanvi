import { Fragment, useEffect, useState } from 'react'
import { core, type NameSet, type Paragraph } from './core/client'

type Status = { state: 'loading' } | { state: 'ready'; ms: number } | { state: 'error'; message: string }

export default function App() {
  const [status, setStatus] = useState<Status>({ state: 'loading' })
  const [nameSets, setNameSets] = useState<NameSet[]>([])
  const [activeSet, setActiveSet] = useState(-1)
  const [input, setInput] = useState('')
  const [document, setDocument] = useState<Paragraph[]>([])
  const [busy, setBusy] = useState(false)

  useEffect(() => {
    const started = performance.now()
    core
      .init()
      .then(async () => {
        setNameSets(await core.nameSets())
        setActiveSet(await core.activeNameSet())
        setStatus({ state: 'ready', ms: performance.now() - started })
      })
      .catch((error: unknown) => setStatus({ state: 'error', message: String(error) }))
  }, [])

  async function translate() {
    setBusy(true)
    setDocument(await core.convert(input))
    setBusy(false)
  }

  async function chooseNameSet(id: number) {
    await core.loadNameSet(id)
    setActiveSet(id)
    if (input) await translate()
  }

  const ready = status.state === 'ready'

  return (
    <div className="app">
      <header>
        <h1>Hanvi</h1>
        <select value={activeSet} disabled={!ready} onChange={(e) => chooseNameSet(Number(e.target.value))}>
          <option value={-1}>No name set</option>
          {nameSets.map((set) => (
            <option key={set.id} value={set.id}>
              {set.title}
            </option>
          ))}
        </select>
        <span className="status">
          {status.state === 'loading' && 'Loading dictionaries…'}
          {status.state === 'ready' && `Dictionaries loaded in ${(status.ms / 1000).toFixed(1)}s`}
          {status.state === 'error' && status.message}
        </span>
      </header>

      <main>
        <section className="pane">
          <textarea
            value={input}
            placeholder="Paste Chinese text here"
            onChange={(e) => setInput(e.target.value)}
          />
          <button disabled={!ready || busy || !input} onClick={translate}>
            {busy ? 'Converting…' : 'Convert'}
          </button>
        </section>

        <section className="pane output">
          {document.map((paragraph, p) => (
            <p key={p}>
              {paragraph.map((token) => (
                <Fragment key={token.id}>
                  {token.vnSpace && ' '}
                  <span className={token.isRule ? 'token rule' : 'token'} title={`${token.cn}\n${token.sv}`}>
                    {token.vn}
                  </span>
                </Fragment>
              ))}
            </p>
          ))}
        </section>
      </main>
    </div>
  )
}
