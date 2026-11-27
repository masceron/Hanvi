import react from '@vitejs/plugin-react'
import { defineConfig } from 'vite'
import { VitePWA } from 'vite-plugin-pwa'

// https://vite.dev/config/
export default defineConfig({
  plugins: [
    react(),
    VitePWA({
      registerType: 'prompt',
      includeAssets: ['favicon.ico', 'apple-touch-icon.png', 'sw-share.js'],
      manifest: {
        name: 'Hanvi',
        short_name: 'Hanvi',
        description: 'Chinese-Vietnamese conversion reader',
        theme_color: '#1c1b22',
        background_color: '#1e1e1e',
        display: 'standalone',
        scope: '/',
        start_url: '/',
        share_target: {
          action: '/share-target',
          method: 'POST',
          enctype: 'multipart/form-data',
          params: {
            title: 'title',
            text: 'text',
            url: 'url',
            files: [
              {
                name: 'file',
                accept: ['text/plain', '.txt'],
              },
            ],
          },
        },
        icons: [
          {
            src: 'pwa-192x192.png',
            sizes: '192x192',
            type: 'image/png',
          },
          {
            src: 'pwa-512x512.png',
            sizes: '512x512',
            type: 'image/png',
          },
        ],
      },
      workbox: {
        importScripts: ['sw-share.js'],
        globPatterns: ['**/*.{js,css,html,wasm,json,ocd2,gz}'],
        // dict.db.gz (~14 MB) exceeds Workbox's 2 MiB default; match Cloudflare Pages' 25 MiB asset limit.
        maximumFileSizeToCacheInBytes: 25 * 1024 * 1024,
        cleanupOutdatedCaches: true,
      },
    }),
  ],
  worker: { format: 'es' },
})
