import React, { useState, useEffect } from 'react'
import { marked } from 'marked'
import hljs from 'highlight.js'
import 'highlight.js/styles/github-dark.css'

// Configure marked to use highlight.js
marked.setOptions({
  highlight: function(code, lang) {
    if (lang && hljs.getLanguage(lang)) {
      return hljs.highlight(code, { language: lang }).value
    }
    return hljs.highlightAuto(code).value
  },
  breaks: true,
  gfm: true
})

function App() {
  const [readme, setReadme] = useState('Loading README...')
  const [error, setError] = useState(null)

  useEffect(() => {
    const fetchReadme = async () => {
      try {
        const response = await fetch(
          'https://raw.githubusercontent.com/swipswaps/quadrf-android/main/README.md'
        )
        if (!response.ok) throw new Error(`HTTP ${response.status}`)
        const text = await response.text()
        setReadme(text)
      } catch (err) {
        setError('Failed to load README. Please try again later.')
        console.error(err)
      }
    }
    fetchReadme()
  }, [])

  return (
    <div className="container">
      <header>
        <h1>Quad‑RF Android</h1>
        <p>Replace your Raspberry Pi with an Android phone – SDR on the go</p>
        <hr />
      </header>
      <main>
        {error ? (
          <div className="error">{error}</div>
        ) : (
          <div
            className="markdown-body"
            dangerouslySetInnerHTML={{ __html: marked.parse(readme) }}
          />
        )}
      </main>
      <footer>
        <p>
          <a href="https://github.com/swipswaps/quadrf-android">GitHub</a> &middot;
          <a href="https://crowdsupply.com/quad-rf">Quad‑RF</a>
        </p>
      </footer>
    </div>
  )
}

export default App
