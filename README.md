# NAT Language v3.5

A simple, English-style compiled programming language written in pure C.
Designed to read like natural English — no symbols where words will do.

---

## Build

```bash
gcc *.c -o nat.exe
```

## Run

```bash
./nat.exe hello.nat
./nat.exe myscript.nat
```

---

## What's New in v3.5

- **Module system** — `use math.tree` imports a `.tree` library file
- **`.tree` files** — contain function definitions only, cleanly separated from logic
- **Standard library** — 4 built-in `.tree` files included
- **`create graph`** — generates interactive HTML charts instantly
- **`newline.` / `skip.`** — print a blank line (aliases of each other)
- **`online`** — print array elements on one line

### Standard Library `.tree` files

| File | Contents |
|------|----------|
| `math.tree` | `sqrt`, `pow`, `log`, `sin`, `cos`, `tan`, `PI`, `E` and more |
| `geometry.tree` | Area/perimeter for all 2D/3D shapes, vectors, coordinates |
| `string.tree` | `capitalize`, `count_words`, `pad_left`, `starts_with`, `ends_with` and more |
| `convert.tree` | km↔miles, °C↔°F, kg↔lbs, radians↔degrees and more |

---

## Language Reference

### Variables
```nat
let x be 42.
let name be "Nisarg".
let x, y, z.
```

### Constants — `fix`
```nat
fix MAX be 100.
fix PI be 3.14159.
```

### Show (print)
```nat
show x.
show "Hello " and name.
newline.
skip.
```

### Using the standard library
```nat
use math.tree

show sqrt(144).
show pow(2, 10).
show sin(90).
show PI.
```

```nat
use geometry.tree

show area_rect(5, 3).
show area_circle(7).
show perimeter_rect(5, 3).
```

```nat
use string.tree

let s be "hello world".
show capitalize(s).
show count_words(s).
show starts_with(s, "hello").
```

```nat
use convert.tree

show km_to_miles(10).
show celsius_to_fahrenheit(100).
show kg_to_lbs(70).
```

### Create graph
```nat
create graph "My Data":
    type line.
    label "Jan" value 10.
    label "Feb" value 25.
    label "Mar" value 18.
    label "Apr" value 40.
end.
```

Opens an interactive HTML chart in your browser. Supported types: `line`, `bar`, `pie`, `scatter`, `area`.

### Arrays
```nat
let nums are 10 20 30 40 50.
show nums[0].
show first of nums.
show last of nums.
show count of nums.
```

### Each loop
```nat
let fruits are "apple" "mango" "banana".
each item in fruits:
    show item.
end.

// Print on one line
show each item in fruits online.
```

### Functions
```nat
make add with a b inside:
    give a + b.
end.

show add(3, 4).
```

### For loop
```nat
repeat i from 1 to 5:
    show i.
end.
```

### Repeat N times
```nat
repeat 5 times:
    show "Hi".
end.
```

### While loop
```nat
let i be 0.
while i < 10:
    show i.
    i be i + 1.
end.
```

### If / Else / Else If
```nat
if x > 10:
    show "big".
else if x > 5:
    show "medium".
else:
    show "small".
end.
```

### File I/O (basic)
```nat
// Write and read files
write "Hello" to "notes.txt".
read "notes.txt".
```

### Input
```nat
ask for name.
show "Hello " and name.
```

### Comments
```nat
// This is a comment
```

---

## Complete Example

```nat
use math.tree
use geometry.tree

fix RADIUS be 7.

show "Circle with radius " and RADIUS.
show "Area: " and area_circle(RADIUS).
show "Circumference: " and circumference_circle(RADIUS).
newline.
show "Square root of area: " and sqrt(area_circle(RADIUS)).
```

---

## File Structure

```
nat_language_v3.5/
├── compiler/
│   ├── main.c
│   ├── parser.c
│   ├── eval.c
│   ├── exec.c
│   ├── tokenizer.c
│   ├── nat.h
│   └── errors.h
├── lib/
│   ├── math.tree
│   ├── geometry.tree
│   ├── string.tree
│   └── convert.tree
├── hello.nat
├── examples.nat
├── graph_test.nat
└── README.md
```

| File | Role |
|------|------|
| `nat.h` | Types, constants, global state externs |
| `tokenizer.c` | Source text → flat token array |
| `parser.c` | Tokens → AST (recursive descent) |
| `eval.c` | Evaluate expression nodes → value |
| `exec.c` | Execute statement nodes, manage scope |
| `main.c` | Entry point, `fix` pre-pass, global state |
| `errors.h` | Helper error system |
| `lib/*.tree` | Standard library modules |
