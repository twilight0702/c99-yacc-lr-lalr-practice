import { existsSync, readdirSync, readFileSync, writeFileSync } from 'node:fs'
import { resolve } from 'node:path'

const before = '(/* #__PURE__ */ {'
const after = '({'
const standalonePureLine = /^\s*\/\*\s*#__PURE__\s*\*\/\s*$/gm
const pnpmDir = resolve(process.cwd(), 'node_modules/.pnpm')

if (!existsSync(pnpmDir)) {
  console.log('[fix-vueuse-pure-annotation] .pnpm directory not found, skip')
  process.exit(0)
}

const candidates = readdirSync(pnpmDir)
  .filter((name) => name.startsWith('@vueuse+core@'))
  .map((name) => resolve(pnpmDir, name, 'node_modules/@vueuse/core/dist/index.js'))
  .filter((p) => existsSync(p))

if (candidates.length === 0) {
  console.log('[fix-vueuse-pure-annotation] @vueuse/core dist file not found, skip')
  process.exit(0)
}

let patched = 0
for (const target of candidates) {
  const source = readFileSync(target, 'utf8')
  if (!source.includes(before) && !/^\s*\/\*\s*#__PURE__\s*\*\/\s*$/m.test(source))
    continue

  const fixed = source
    .replaceAll(before, after)
    .replace(standalonePureLine, '')
  writeFileSync(target, fixed, 'utf8')
  patched += 1
}

if (patched === 0)
  console.log('[fix-vueuse-pure-annotation] pattern not found, skip')
else
  console.log(`[fix-vueuse-pure-annotation] patched ${patched} file(s)`)
