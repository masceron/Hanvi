const SHARE_CACHE = 'hanvi-share-target'
const SHARE_KEY = '/shared-payload'

self.addEventListener('fetch', (event) => {
  const url = new URL(event.request.url)
  if (event.request.method !== 'POST' || url.pathname !== '/share-target') return

  event.respondWith(
    (async () => {
      try {
        const formData = await event.request.formData()
        const file = formData.get('file')
        let title = String(formData.get('title') ?? '').trim()
        let text = ''

        if (file && typeof file === 'object' && typeof file.text === 'function') {
          text = await file.text()
          if (!title && 'name' in file && typeof file.name === 'string') {
            title = file.name
          }
        } else {
          const sharedText = String(formData.get('text') ?? '')
          const sharedUrl = String(formData.get('url') ?? '')
          text = [sharedText, sharedUrl].filter((part) => part.trim()).join('\n')
        }

        const cache = await caches.open(SHARE_CACHE)
        await cache.put(
          SHARE_KEY,
          new Response(JSON.stringify({ title, text }), {
            headers: { 'Content-Type': 'application/json' },
          }),
        )
      } catch {
        // Ignore malformed share payloads and redirect cleanly.
      }

      return Response.redirect('/?share=1', 303)
    })(),
  )
})
