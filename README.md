# NAT — Natural Abstract Tree

A simple, English-style compiled programming language written in pure C.
Designed to read like natural English — no symbols where words will do.

This language was created by me (Nisarg) with the help of Claude Sonnet.
The earliest versions V1.0 and V2.0 of NAT were written by me, but they had so many bugs that I had to bring in Claude to make it stable. Claude restructured the code and edited it into a better, stable form.

When I first started learning to code, I was curious about how a programming language was created — how Dennis Ritchie created C, and why C performs better than other languages. This led me to research compilers and at the mid-semester of my second year (yup, I am a college student), I thought of the idea of creating my own language: simple to understand and fun to write. I built V1.0 and V2.0 — very basic tokenizer and parser, all in one `main.c` file, which didn't allow multi-line code and had broken identifiers. This made me give up on the project — until I realized this is the era of AI. So I uploaded my broken `main.c` to Claude, and to my surprise, it understood the method, the code structure, the ideology, and improved it into separate `parser.c`, `tokenizer.c` and other files. From there the first stable version was born: **v3.0**. Claude also became my coding partner — I code, Claude improves it.

I hope the devs will check this project.

---

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

## Why NAT?

- **Reads like English** — `let x be 10.` instead of `int x = 10;`
- **Friendly errors** — tells you exactly what went wrong and how to fix it
- **Batteries included** — standard library for math, geometry, strings, and unit conversion
- **Module system** — import `.tree` files with `use math.tree`
- **File I/O** — read, write, append, insert, delete lines from files
- **Graph output** — `create graph` generates HTML charts instantly

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

## Quick Start

**hello.nat**
```nat
show "Hello, World!".
```

Run it:
```
nat hello.nat
```

**Variables and math:**
```nat
let x be 10.
let y be 20.
let sum be x + y.
show sum.
```

**Functions:**
```nat
make greet with name inside:
    show "Hello, " and name.
end.

greet "Nisarg".
```

**Loops:**
```nat
repeat 5 times:
    show "NAT is great.".
end.

repeat i from 1 to 10:
    show i.
end.
```

**Arrays:**
```nat
let fruits are "apple" "mango" "banana".
each item in fruits:
    show item.
end.
```

**File I/O:**
```nat
write "Hello" to "notes.txt".
append "World" to "notes.txt".
read "notes.txt".

each line in file "notes.txt":
    show line.
end.
```

**Using the standard library:**
```nat
use math.tree
show sqrt 144.
show pow 2, 10.

use geometry.tree
show area_rect 5, 3.
```

---

## Standard Library

| File | What's inside |
|------|--------------|
| `math.tree` | `sqrt`, `pow`, `factorial`, `log`, `sin`, `cos`, `PI`, `E`, `TAU` and more |
| `geometry.tree` | Area and perimeter for 2D/3D shapes, coordinate geometry, vectors |
| `string.tree` | `title_of`, `word_count`, `pad_left`, `starts_with`, `ends_with` and more |
| `convert.tree` | km↔miles, °C↔°F, kg↔lbs, radians↔degrees and more |

---

## Project Structure

```
NAT/
├── compiler/       ← source code (C)
│   ├── main.c
│   ├── parser.c
│   ├── eval.c
│   ├── exec.c
│   ├── tokenizer.c
│   ├── nat.h
│   └── errors.h
├── lib/            ← standard library (.tree files)
│   ├── math.tree
│   ├── geometry.tree
│   ├── string.tree
│   └── convert.tree
├── examples/       ← example .nat programs
├── nat.exe         ← Windows binary (latest release)
└── README.md
```

---

## Building from Source

Requires GCC.

```bash
cd compiler
gcc *.c -o ../nat.exe -lm
```

---

## Language Syntax at a Glance

```nat
// Variables
let x be 10.
let name be "NAT".
let x, y, z.           // declare multiple — default 0

// Assignment
x be 20.
x = 20.                // = also works

// Conditionals
if x is greater than 5:
    show "big".
else:
    show "small".
end.

// While loop
while x is greater than 0:
    x be x - 1.
end.

// Functions — brackets optional
make add with a b inside:
    give a + b.
end.

show add 3, 4.
show add(3, 4).

// Min / Max
show larger(12, 45, 677, 2).      // 677
show smallest(1, 45, 6, 0.008).   // 0.008
show larger of 10, 20, 5.         // 20

// Constants
fix PI be 3.14159.

// Ask user input
ask for name.
show "You entered: " and name.

// File I/O
write "text" to "file.txt".
append "more" to "file.txt".
let line be read "file.txt" at line 1.
if file "file.txt" exists:
    delete file "file.txt".
end.
```

---

## Versions

| Version | Highlights |
|---------|-----------|
| v3.0 | Core language — variables, loops, functions, arrays |
| v3.1 | Bug fixes, `is between X and Y`, built-in functions |
| v3.2 | Parser improvements |
| v3.3 | Helper error system with personality messages |
| v3.4 | Standard library — trim, replace, split, math ops, type checking, random |
| v3.5 | Module system (`.tree` files), `create graph`, `each` loops |
| v3.6 | File I/O — read, write, append, insert, remove, delete, `each line in file` |
| v3.7 | No-bracket calls, `larger`/`smallest`, numeric cache (Phase 1), new directory structure |
| v4.0 | Phase 2 — typed variable storage, performance foundation |

---

## Roadmap

- **v4.0 Phase 2** — typed Variable storage (`double` + `string` union, no more `char[]` for numbers)
- **v4.0 Phase 3** — `nat_runtime` library
- **v4.0 Phase 4** — `nat build` transpile-to-C via bundled TinyCC (zero external compiler needed)
- **v4.5** — Object system, reserved keywords

---

## License

MIT — see [LICENSE](LICENSE)

---
## Ewww Vibe-Coding Sucks

*NAT — Created by Nisarg and Claude, for the devs.*
