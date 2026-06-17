# NAT Language — GitHub Release Notes
# Copy-paste each section when creating a release on GitHub

---

## v3.0 — The Beginning
**Tag:** `v3.0`

The first stable version of NAT. Core language is functional.

### What's in this version
- Variables — `let x be 10.`
- Arithmetic — `+`, `-`, `*`, `/`, `^` (power)
- String output — `show "hello".`
- `show x and y.` — print multiple values on one line
- `if / else / end.` — conditionals
- `repeat N times:` — fixed loop
- `repeat i from 1 to 10:` — range loop
- `while condition:` — while loop
- `make / give / end.` — functions with return values
- Arrays — `let fruits are "apple" "mango".`
- `ask for x.` — user input
- `fix PI be 3.14.` — constants
- Basic error messages

---

## v3.1 — Bug Fixes & Built-ins
**Tag:** `v3.1`

### What's new
- `is between X and Y` syntax for range checks
- Built-in functions wired: `__count__`, `__text__`, `__num__`, `__read__`
- `g_current_line` error tracking — errors now show the correct line number

### Bug fixes
- `else if` single-line expansion fixed
- Error line numbers were off by one in some cases

---

## v3.1.1 — Patch
**Tag:** `v3.1.1`

### Bug fixes
- Further stability fixes to `else if` chaining
- Edge cases in line tracking resolved

---

## v3.2 — Parser Improvements
**Tag:** `v3.2`

### What's new
- Major parser rewrite for more reliable statement parsing
- Improved handling of nested blocks (`if` inside `repeat` etc.)
- More consistent error recovery — parser no longer crashes on unknown syntax, reports cleanly instead

---

## v3.3 — Helper Error Experience
**Tag:** `v3.3`

NAT errors now have personality. Inspired by the Madagascar Penguins.

### What's new
- Complete Helper Error system — every error has a `what went wrong` message AND a `hint` on how to fix it
- Personality words rotate randomly in error messages: **mate**, **bud**, **higher mammal**
- Errors show the exact line number and a helpful suggestion, not just a crash
- `errors.h` — dedicated error handling module added to the codebase

### Example
```
NAT error on line 4: unknown variable 'scroe', mate!
  Hint: did you mean 'score'? declare it first with:  let scroe be 0.
```

---

## v3.4 — Standard Library
**Tag:** `v3.4`

### What's new
- **Standard library functions** built directly into the language:
  - String ops: `trim`, `replace`, `split`, `upper`, `lower`, `length`
  - Math ops: `abs`, `round`, `floor`, `ceil`, `sqrt`, `pow`
  - Type checking: `is_number`, `is_text`
  - Random: `random from A to B`
  - Array ops: `first of`, `last of`, `count of`, `push`, `pop`
  - Clamp: `clamp x between A and B`
- `each item in array:` loop
- `each char in string:` loop
- Number formatting — integers display without `.0`

### Bug fixes
- Scientific notation in `is_number()` now handled correctly
- Logical AND via `g_in_condition` flag fixed
- Bare-word variable vs string literal disambiguation resolved

---

## v3.5 — Module System & Graphs
**Tag:** `v3.5`

### What's new
- **Module system** — `use math.tree` imports a `.tree` file
- `.tree` files contain function definitions only — clean separation of library vs logic
- **Standard library `.tree` files:**
  - `math.tree` — `sqrt`, `pow`, `log`, `sin`, `cos`, `tan`, `PI`, `E` and more
  - `geometry.tree` — area/perimeter for all 2D/3D shapes, vectors, coordinates
  - `string.tree` — `capitalize`, `count_words`, `pad_left`, `starts_with`, `ends_with` and more
  - `convert.tree` — km↔miles, °C↔°F, kg↔lbs, radians↔degrees and more
- **`create graph`** — generates an HTML file with an interactive chart
  - 5 graph types: line, bar, pie, scatter, area
  - Fully self-contained HTML output, opens in any browser
- `newline.` / `skip.` — print a blank line (aliases of each other)
- `online` keyword — print array elements on one line

### Bug fixes
- Tree-importing-tree double-snapshot bug fixed
- Keyword conflicts as parameter names resolved

---

## v3.6 — File I/O
**Tag:** `v3.6`

### What's new
- **Full File I/O system:**
  - `write "text" to "file.txt".` — create or overwrite
  - `write "text" to "file.txt" at line N.` — overwrite a specific line
  - `append "text" to "file.txt".` — add to end
  - `insert "text" to "file.txt" at line N.` — insert, push lines down
  - `remove line N from "file.txt".` — delete a specific line
  - `read "file.txt".` — print file with `line N | content` format
  - `let x be read "file.txt" at line N.` — read a line into a variable
  - `if file "file.txt" exists:` — check file existence
  - `delete file "file.txt".` — delete a file
  - `each line in file "file.txt":` — loop over every line
- **Runtime injection** — `write x inside "file.nat"` injects a variable value at runtime without modifying the file on disk. The file stays unchanged — injection lives only in memory for that run.
- **`=` assignment** — `x = 10.` now works alongside `x be 10.` (backward compatible)
- **Auto-print for bare function calls** — `area_rect(2,5).` now prints the return value when called as a standalone statement, no `show` needed
- Helper errors for all file operations (file not found, permission denied, line out of range)

### Bug fixes
- `sum = num1 + num2.` was incorrectly treated as a function call — fixed
- `area_rect(2,5).` as a bare statement returned 0 instead of printing — fixed

---

## v3.6.1 — Windows Hotfix
**Tag:** `v3.6.1`

### Bug fixes
- **Windows CRLF fix** — files written on Windows were getting `\r\n` line endings injected by the OS in text mode. This caused `read "file.txt"` output to display incorrectly in the terminal (line 1 appearing to vanish due to carriage return)
- All file operations now use **binary mode** (`rb`/`wb`/`ab`) — consistent behavior on Windows and Linux
- Line reading strips both `\r` and `\n` — files written by Notepad or other Windows editors now read cleanly too

> **Windows users on v3.6 should upgrade to v3.6.1.**
