# NAT Language v3.2

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

## What's New in v3.2

- Major parser rewrite — more reliable and consistent statement parsing
- Nested blocks now work correctly (`if` inside `repeat` inside `while` etc.)
- Parser no longer crashes on unknown syntax — reports a clean error instead
- Improved error recovery throughout

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
show 2 ^ 8.
show (x + y) * 2.
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

### English comparisons
```nat
if x is greater than 5:
    show "big".
end.

if x is less than 10:
    show "ok".
end.

if x is not 0:
    show "yes".
end.
```

### Arrays
```nat
let nums are 10 20 30 40 50.
show nums[0].
show nums[4].
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
fix MAX be 10.

make fib with n inside:
    if n < 2:
        give n.
    end.
    give fib(n - 1) + fib(n - 2).
end.

repeat i from 0 to MAX:
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
| `hello.nat` | Full feature showcase |
| `examples.nat` | Fibonacci, factorial, power and more |
