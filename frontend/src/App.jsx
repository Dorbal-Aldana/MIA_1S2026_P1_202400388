import { useState, useRef } from 'react'
import './App.css'

const API_URL = '/api'

export default function App() {
  const [input, setInput] = useState('')
  const [output, setOutput] = useState('')
  const [loading, setLoading] = useState(false)
  const [error, setError] = useState(null)
  const fileInputRef = useRef(null)

  const handleLoadScript = () => {
    fileInputRef.current?.click()
  }

  const handleFileChange = (e) => {
    const file = e.target.files?.[0]
    if (!file) return
    const reader = new FileReader()
    reader.onload = (ev) => {
      setInput(ev.target?.result ?? '')
      setError(null)
    }
    reader.readAsText(file, 'UTF-8')
    e.target.value = ''
  }

  const handleExecute = async () => {
    setError(null)
    setLoading(true)
    setOutput('Ejecutando...\n')
    try {
      const payload = JSON.stringify({ command: input })
      const res = await fetch(`${API_URL}/command`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: payload,
      })
      const text = await res.text()
      let out = ''
      try {
        const json = JSON.parse(text)
        out = json.output ?? text
      } catch {
        out = text
      }
      const outputStr = typeof out === 'string' ? out : JSON.stringify(out)
      setOutput(outputStr || '(El servidor no devolvió salida)')
      if (!res.ok) setError(`HTTP ${res.status}`)
    } catch (err) {
      setError(err.message || 'Error de conexión. ¿Está el backend en marcha (puerto 8080)?')
      setOutput('')
    } finally {
      setLoading(false)
    }
  }

  return (
    <div className="app">
      <header className="header">
        <h1>ExtreamFS</h1>
        <p className="subtitle">Sistema de archivos EXT2 — Proyecto 1 MIA</p>
      </header>

      <main className="main">
        <section className="panel input-panel">
          <div className="panel-header">
            <h2>Entrada de comandos</h2>
            <div className="actions">
              <input
                ref={fileInputRef}
                type="file"
                accept=".smia"
                onChange={handleFileChange}
                style={{ display: 'none' }}
              />
              <button type="button" className="btn btn-secondary" onClick={handleLoadScript}>
                Cargar script (.smia)
              </button>
              <button
                type="button"
                className="btn btn-primary"
                onClick={handleExecute}
                disabled={loading}
              >
                {loading ? 'Ejecutando…' : 'Ejecutar'}
              </button>
            </div>
          </div>
          <textarea
            className="textarea input-area"
            placeholder="# Escriba comandos o cargue un script .smia&#10;mkdisk -size=10 -path=/tmp/disco.mia&#10;# mount -path=/tmp/disco.mia -name=Part1"
            value={input}
            onChange={(e) => setInput(e.target.value)}
            spellCheck={false}
            rows={14}
          />
        </section>

        <section className="panel output-panel">
          <div className="panel-header">
            <h2>Salida de comandos</h2>
          </div>
          {error && (
            <div className="error-banner">
              {error}
            </div>
          )}
          <pre className="textarea output-area">
            {output || (loading ? 'Ejecutando...' : 'La salida aparecerá aquí.')}
          </pre>
        </section>
      </main>

      <footer className="footer">
        <span>Backend: C++ · Frontend: React + Vite</span>
      </footer>
    </div>
  )
}
