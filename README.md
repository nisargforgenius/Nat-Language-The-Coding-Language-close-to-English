# NAT — Natural Abstract Tree

> **Easy coding for developers.**
> A compiled, English-style language built in C.

NAT lets you write code that reads like plain English — no semicolons, no curly braces, no boilerplate. Write `.nat` files, run them with `nat program.nat`. Clear, friendly errors tell you exactly what went wrong and how to fix it.

```nat
let name be "Nisarg".
let age be 21.

show "Hello, " and name.
show "Age: " and age.

if age is greater than 18:
    show "Access granted.".
end.
```

---

## Installation (Windows)

1. Download the latest release zip from [Releases](../../releases)
2. Extract to `C:\NAT\`
3. Add `C:\NAT` to your System PATH
4. Open a terminal anywhere and run:

```
nat hello.nat
```

> **Linux/Mac:** compile from source — `cd compiler && gcc *.c -o nat -lm`

---

## Project Structure

```
NAT/
├── compiler/       ← C source files (build NAT from here)
├── examples/       ← example and test .nat programs
├── lib/            ← standard library (.tree files)
├── nat.exe         ← Windows binary
├── hello.nat       ← full language showcase
└── README.md
```

**Build command:**
```bash
cd compiler
gcc *.c -o ../nat.exe -lm
```

---

## Quick Start

```nat
// hello.nat
show "Hello, World!".
```

```
nat hello.nat
```

---

## Language at a Glance

### Variables & Assignment
```nat
let x be 10.
let name be "Nisarg".
let x, y, z.        // declare multiple — default 0

x be 20.            // reassign with 'be'
x = 20.             // reassign with '=' — both work
```

### Math
```nat
show 10 + 3.
show 2 ^ 8.
show (x + y) * 2.
show larger(12, 45, 677, 2).     // 677
show smallest(1, 45, 6, 0.008).  // 0.008
show larger of 10, 20, 5.        // 20  — 'of' syntax also works
```

### Functions
```nat
make add with a b inside:
    give a + b.
end.

show add(3, 4).      // with brackets
show add 3, 4.       // without brackets — both work
```

### Loops
```nat
repeat i from 1 to 10:
    show i.
end.

repeat 5 times:
    show "NAT!".
end.

while x > 0:
    x = x - 1.
end.
```

### Conditions
```nat
if score >= 90:
    show "A".
else if score >= 80:
    show "B".
else:
    show "C".
end.

if x is between 1 and 10:
    show "in range".
end.
```

### Arrays
```nat
let fruits are "apple" "mango" "banana".
show first of fruits.
show last of fruits.
show count of fruits.

each item in fruits:
    show item.
end.
```

### File I/O
```nat
write "Hello" to "notes.txt".
append "World" to "notes.txt".
read "notes.txt".

each line in file "notes.txt":
    show line.
end.

if file "notes.txt" exists:
    delete file "notes.txt".
end.
```

### Standard Library
```nat
use math.tree
show sqrt 144.
show pow 2, 10.
show factorial 5.

use geometry.tree
show area_rect 5, 3.
show area_circle 7.

use string.tree
show title_of "hello world".

use convert.tree
show cel_to_fahr 100.
```

### Comments
```nat
// single line
/* multi
   line */
# hash style — also works
```

---

## Standard Library

| File | Contents |
|------|----------|
| `math.tree` | `sqrt`, `pow`, `factorial`, `gcd`, `lcm`, `sin_deg`, `cos_deg`, `ln`, `log10`, `PI`, `E`, `TAU` |
| `geometry.tree` | Area/perimeter for circles, rectangles, triangles, 3D shapes, vectors |
| `string.tree` | `title_of`, `word_count`, `starts_with`, `ends_with`, `pad_left`, `is_alpha` |
| `convert.tree` | `cel_to_fahr`, `km_to_mile`, `kg_to_lb`, `deg_to_rad` and more |

---

## Versions

| Version | Highlights |
|---------|-----------|
| v3.0 | Core language |
| v3.1 | `is between`, built-in functions |
| v3.2 | Parser rewrite |
| v3.3 | Helper error system with personality messages |
| v3.4 | Standard library builtins |
| v3.5 | Module system (`.tree`), `create graph` |
| v3.6 | File I/O, runtime injection, `=` assignment |
| v3.6.1–3.6.4 | Windows CRLF fix, comment system, `is between`, `random between`, example syntax modernization |
| v3.7 | No-bracket function calls, Phase 1a/1b numeric cache, `larger`/`smallest`, proper directory structure |
| v4.0 | Phase 2 — typed variable storage (in progress) |

---

## Roadmap

- **v4.0 Phase 2** — typed Variable storage (`double` + `string` union, no more char[] for numbers)
- **v4.0 Phase 3** — `nat_runtime` library
- **v4.0 Phase 4** — `nat build` transpile-to-C via bundled TinyCC
- **v4.5** — Object system, reserved keywords

---

## License

MIT — see [LICENSE](LICENSE)

---

*Founder: Nisarg | Co-Founder: Claude Sonnet*
*NAT — the guy in the chair. C is Spider-Man, NAT handles the rest.*
