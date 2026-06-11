# NAT Language v3.3 — Error Experience Improved

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

## What's New in v3.3

NAT errors now have personality. Every error tells you exactly what went wrong AND how to fix it — with a friendly nudge from the Helper Error system.

- **Complete Helper Error system** — every error has a `what went wrong` message and a `hint`
- **Personality words** rotate randomly in error messages: **mate**, **bud**, **higher mammal**
- Errors show the exact line number
- `errors.h` — dedicated error handling module added to the codebase

### Example error
```
NAT error on line 4: unknown variable 'scroe', mate!
  Hint: did you mean 'score'? declare it first with:  let scroe be 0.
```

---

## Language Reference

### Variables
```nat
let x be 42.
let name be "Nisarg".
let x, y, z.        // declare multiple, default to 0
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
show 2^8.
show (x + y) * 2.
```

### Functions
```nat
make greet with name inside:
    show "Hello " and name.
end.

greet("Nisarg").
```

Return a value with `give`:
```nat
make square with n inside:
    give n * n.
end.

show square(7).
```

### For loop — inclusive
```nat
repeat i from 1 to 5:
    show i.
end.

repeat i from 0 to 10 step 2:
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

### Arrays
```nat
let fruits are "apple" "mango" "banana".
show fruits[0].
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
make fib with n inside:
    if n < 2:
        give n.
    end.
    give fib(n - 1) + fib(n - 2).
end.

repeat i from 0 to 10:
    show fib(i).
end.
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
| `errors.h` | Helper error system with personality messages |
| `hello.nat` | Full feature showcase |
| `examples.nat` | Fibonacci, factorial, power and more |
