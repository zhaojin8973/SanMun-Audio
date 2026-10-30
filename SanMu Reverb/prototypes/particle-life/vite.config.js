import { defineConfig } from 'vite';

export default defineConfig({
  build: { rolldownOptions: { input: { life: 'index.html', studio: 'studio.html', palette: 'palette.html' } } },
});
