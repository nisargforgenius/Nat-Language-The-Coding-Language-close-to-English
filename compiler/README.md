# NAT — Natural Abstract Tree

> **Easy coding for developers.**
> A simple, English-style compiled language written in C.

NAT lets you write code that reads like plain English — no semicolons, no curly braces, no boilerplate. It compiles and runs your `.nat` files directly, with clear, friendly error messages that actually tell you what went wrong.

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
2. Extract the folder anywhere (e.g. `C:\NAT\`)
3. Add the folder to your system PATH
4. Open a terminal and run:

```
nat hello.nat
```

> Linux/Mac: compile from source with `gcc *.c -o nat -lm`

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

greet("Nisarg").
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
show pow(2, 10).

use geometry.tree
show area_rect 5, 3.
```

---

## Standard Library

| File | What's inside |
|------|--------------|
| `math.tree` | `sqrt`, `pow`, `log`, `sin`, `cos`, `abs`, `floor`, `ceil`, `PI`, `E` and more |
| `geometry.tree` | Area and perimeter for 2D/3D shapes, coordinate geometry, vectors |
| `string.tree` | `capitalize`, `count_words`, `pad_left`, `starts_with`, `ends_with` and more |
| `convert.tree` | km↔miles, °C↔°F, kg↔lbs, radians↔degrees and more |

---

## Project Structure

```
nat-language/
├── compiler/       ← source code (C)
│   ├── main.c
│   ├── parser.c
│   ├── eval.c
│   ├── exec.c
│   ├── tokenizer.c
│   ├── nat.h
│   └── errors.h
├── tree/           ← standard library (.tree files)
│   ├── math.tree
│   ├── geometry.tree
│   ├── string.tree
│   └── convert.tree
├── examples/       ← example .nat programs
│   ├── hello.nat
│   ├── examples.nat
│   └── ...
├── nat.exe         ← Windows binary (latest release)
└── README.md
```

---

## Building from Source

Requires GCC.

```bash
cd compiler
gcc *.c -o nat.exe -lm
```

---

## Language Syntax at a Glance

```nat
// Variables
let x be 10.
let name be "NAT".
let x, y, z.           // declare multiple

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

// Functions
make add with a b inside:
    give a + b.
end.

let result be add(3, 4).

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

---

## Roadmap

- **v3.7** — Polish pass, `inline.` statement, Windows installer (.bat)
- **v4.0** — Object system, direct memory management
- **v4.5** — Reserved keywords, VM

---

## License

MIT — see [LICENSE](LICENSE)

---

*NAT — the guy in the chair. C is Spider-Man, NAT handles the rest.*
