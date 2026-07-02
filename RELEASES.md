# NAT Language — Release Notes

---

## v3.0 — The Beginning
**Tag:** `v3.0`
First stable version. Core language functional.
- Variables, arithmetic, strings, constants (`fix`)
- `if/else`, `while`, `repeat N times`, `repeat i from A to B`
- Functions with `make/give/end`
- Arrays, `ask for` input
- Basic error messages

---

## v3.1 — Bug Fixes & Built-ins
**Tag:** `v3.1`
- `is between X and Y` range check
- Built-in type functions wired
- Error line numbers now correct

---

## v3.1.1 — Patch
**Tag:** `v3.1.1`
- `else if` chaining edge cases fixed
- Stability improvements

---

## v3.2 — Parser Improvements
**Tag:** `v3.2`
- Major parser rewrite — more reliable statement parsing
- Nested blocks work correctly in all combinations
- Clean error recovery on unknown syntax

---

## v3.3 — Helper Error Experience
**Tag:** `v3.3`
NAT errors now have personality.
- Every error has a `what went wrong` + `hint` message
- Personality words: **mate**, **bud**, **higher mammal**
- `errors.h` dedicated error module added

---

## v3.4 — Standard Library
**Tag:** `v3.4`
- String ops: `trim`, `replace`, `split`, `upper`, `lower`, `length`
- Math ops: `abs`, `round`, `floor`, `ceil`, `sqrt`, `pow`
- Type checking: `is number`, `is text`
- `random from A to B`
- Array ops: `first of`, `last of`, `count of`, `push`, `pop`, `clamp`
- `each item in array:` and `each char in string:` loops
- Numbers display without `.0` for whole values

---

## v3.5 — Module System & Graphs
**Tag:** `v3.5`
- `use math.tree` — module import system
- 4 standard library `.tree` files: `math`, `geometry`, `string`, `convert`
- `create graph` — generates interactive HTML charts (5 graph types)
- `newline.` / `skip.` — blank line output
- `online` — print array elements on one line

---

## v3.6 — File I/O
**Tag:** `v3.6`
- Full file I/O: `write`, `append`, `insert`, `remove line`, `read`, `delete`
- `read "file.txt" at line N` — read specific line into variable
- `each line in file "x.txt":` — loop over file lines
- `write x inside "file.nat"` — runtime injection (no disk write)
- `=` assignment works alongside `be`
- Bare function calls auto-print return value

---

## v3.6.1 — Windows CRLF Fix
**Tag:** `v3.6.1`
- All file ops switched to binary mode — no OS-injected `\r\n`
- Line reading strips both `\r` and `\n`
- **Windows users on v3.6 should upgrade**

---

## v3.6.2 — Comment System Fixed
**Tag:** `v3.6.2`
- `/* single line block */` comments now work
- Multi-line `/* ... */` block comments now work
- `//` and `#` single-line comments confirmed working

---

## v3.6.3 — Syntax Modernization
**Tag:** `v3.6.3`
- `nat.exe` banner updated: "NAT Language **Compiler** v3.6.3"
- Founder: Nisarg | Co-Founder: Claude Sonnet added to banner
- `hello.nat` and `examples.nat` rewritten to modern v3.6 syntax
- `is between A and B` added as alias for `in middle of A and B`
- `pow(base, exp)` added to `math.tree`

---

## v3.6.4 — Regression Suite & Fixes
**Tag:** `v3.6.4`
- 74-test regression suite added (3 files: new syntax, legacy compat, type coercion)
- `random between A and B` added as alias for `random from A to B`
- Large decimal numbers no longer collapse to scientific notation (`1000000.5` not `1e+06`)
- Golden baseline outputs saved for Phase 1 safety net

---

## v3.7 — No-Bracket Calls & Phase 1 Numeric Cache
**Tag:** `v3.7`

### New features
- **No-bracket function calls** — `show sqrt 144.` works for user-defined AND tree-imported functions
- Multi-arg no-bracket: `show add 3, 4.` — comma-separated args
- `larger(a,b,c,...)` / `larger of a,b,c` / `larger a,b,c` — returns maximum of any number of values
- `smallest(a,b,c,...)` / `smallest of a,b,c` — returns minimum of any number of values
- `lib/` search now uses `nat.exe` binary location — `use math.tree` works from any folder when `C:\NAT` is in PATH
- `NAT_HOME` environment variable support for custom install paths

### v4.0 Phase 1 (performance foundation)
- `Variable` struct gains `is_num` + `num_value` numeric cache — numbers stored as `double`, not just as strings
- `set_var()` and all direct variable write sites call `cache_numeric()` on every assignment
- `eval_num()` fast-path function — arithmetic subtrees evaluated in pure `double` with zero string conversion at intermediate tree levels
- Eliminates `atof()`/`sprintf()` round-trips on the hot numeric path

### Directory structure
- Reorganized to `compiler/`, `examples/`, `lib/` layout
- Build: `cd compiler && gcc *.c -o ../nat.exe -lm`

### Bug fixes
- Stale variable slot reuse bug fixed — calling `sqrt()` then `factorial()` no longer corrupts parameters
- Keyword-named user functions (`make add`) now work with no-bracket call syntax

---

## v4.0 — Phase 2: Typed Variable Storage *(in progress)*
**Tag:** `v4.0`

### What's changing
- `Variable` struct: `char value[MAX_STR]` split into typed storage
  - `ValueType type` — `VAL_NUM` or `VAL_STR`
  - `double num_value` — used when type is numeric (replaces `atof` on every read)
  - `char str_value[MAX_STR]` — used when type is string
- Arithmetic operations read `num_value` directly — zero string conversion on the hot path
- `show` converts to string only at print time
- All 74 regression tests must pass with identical output

### Why this matters
The current architecture stores everything as `char[]` — even `let x be 10.` becomes `"10"` in memory. Every `x + y` does `atof("10") + atof("20")` → `30.0` → `sprintf` back to `"30"`. Phase 2 eliminates this entirely for numeric variables — the foundation for `nat build` (Phase 4) to generate correct typed C code.
