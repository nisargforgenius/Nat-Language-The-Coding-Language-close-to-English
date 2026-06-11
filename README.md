# NAT Language v3.4

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

## What's New in v3.4

Big release — standard library functions are now built directly into the language.

- **String ops** — `trim`, `replace`, `split`, `upper`, `lower`, `length`
- **Math ops** — `abs`, `round`, `floor`, `ceil`, `sqrt`, `pow`
- **Type checking** — `is_number`, `is_text`
- **Random numbers** — `random from A to B`
- **Array ops** — `first of`, `last of`, `count of`, `push`, `pop`, `clamp`
- **`each item in array:`** loop
- **`each char in string:`** loop
- Number formatting — integers display without `.0`

### Bug fixes
- Scientific notation in `is_number()` now handled correctly
- Logical AND fixed in complex conditions
- Bare-word variable vs string literal disambiguation resolved

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
show x + y.
```

### Math — `+ - * / % ^`
```nat
show 10 + 3.
show 2 ^ 8.
```

### Built-in math functions
```nat
show abs(-5).
show round(3.7).
show floor(3.9).
show ceil(3.1).
show sqrt(144).
show pow(2, 10).
```

### String functions
```nat
let name be "  hello  ".
show trim of name.
show upper of name.
show lower of name.
show length of name.
show replace "hello" with "world" in name.
```

### Type checking
```nat
if is_number(x):
    show "it's a number".
end.
```

### Random
```nat
let n be random from 1 to 100.
show n.
```

### Arrays
```nat
let fruits are "apple" "mango" "banana".
show first of fruits.
show last of fruits.
show count of fruits.
push "grape" to fruits.
pop from fruits.
```

### Each loop — arrays and strings
```nat
let fruits are "apple" "mango" "banana".
each item in fruits:
    show item.
end.

let word be "hello".
each char in word:
    show char.
end.
```

### Clamp
```nat
let x be clamp 150 between 0 and 100.
show x.    // 100
```

### Split
```nat
let parts be split "hello world" by " ".
each item in parts:
    show item.
end.
```

### Functions
```nat
make add with a b inside:
    give a + b.
end.

show add(3, 4).
```

### For loop — inclusive
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

### Range check
```nat
if x is between 1 and 10:
    show "in range".
end.
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
let scores are 85 92 78 96 88.

show "Scores:".
each score in scores:
    if score is greater than 90:
        show score and " — excellent!".
    else:
        show score and " — good.".
    end.
end.

show "Total: " and count of scores.
show "Top score: " and last of scores.
```

---

## File Structure

| File | Role |
|------|------|
| `nat.h` | Types, constants, global state externs |
| `tokenizer.c` | Source text → flat token array |
| `parser.c` | Tokens → AST (recursive descent) |
| `eval.c` | Evaluate expression nodes → value |
| `exec.c` | Execute statement nodes, manage scope |
| `main.c` | Entry point, `fix` pre-pass, global state |
| `errors.h` | Helper error system |
| `hello.nat` | Full feature showcase |
| `examples.nat` | Fibonacci, factorial, power and more |
