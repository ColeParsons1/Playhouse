import { defineConfig } from 'vite'
import react from '@vitejs/plugin-react'
import process from 'node:process'

// Django backend used in development (start.sh runs it on port 8001).
// Override with e.g. `BACKEND_URL=http://localhost:8000 npm run dev`.
const BACKEND_URL = process.env.BACKEND_URL || 'http://localhost:8001'

// https://vite.dev/config/
export default defineConfig({
  plugins: [react()],
  server: {
    // The frontend calls the API at the relative path /api (production serves
    // it through nginx). In development, forward those calls to Django.
    proxy: {
      '/api': { target: BACKEND_URL, changeOrigin: true },
    },
  },
})
