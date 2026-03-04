import { defineConfig } from 'vitest/config';

export default defineConfig({
    test: {
        include: ['src/js/__tests__/**/*.test.js'],
    },
});
