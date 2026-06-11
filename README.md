# NAT Language v3.0

A simple, English-style interpreted programming language written in pure C.
Designed to read like natural English — no symbols where words will do.

---

## Build

```bash
gcc *.c -o nat
```

Windows (MinGW):
```bash
gcc *.c -o nat.exe
```

## Run

```bash
./nat hello.nat
./nat myscript.nat
```

---

## What Changed in v3.0

| Old (v2)             | New (v3)                     | Notes                        |
|----------------------|------------------------------|------------------------------|
| `let x = int(5).`    | `let x be num(5).`           | `be` replaces `=`            |
| `int()` / `float()`  | `num()`                      | One type for all numbers     |
| `let name = string(Nisarg).` | `let name be Nisarg.` | Bare word = string     |
| *(not available)*    | `fix PI 3.14`                | Constants, no dot needed     |
| *(not available)*    | `add x with y to z.`         | English arithmetic assignment|
| `repeat i from 1 to 5` → prints 1–4 | `repeat i from 1 to 5` → prints 1–5 | Now **inclusive** |

Old syntax (`= int() float() string()`) still works for backwards compatibility.

---

## Language Reference

### Variables
```nat
let x    be num(42).
let pi   be num(3.14).
let name be Nisarg.
let msg  be "Hello World".
```

### Constants  (`fix` — immutable, no dot needed)
```nat
fix MAX 100
fix PI 3.14159
fix GREETING Hello
```
Constants can be used anywhere a value is expected:
```nat
show(PI).
show(PI * r * r).
repeat i from 1 to MAX step 1:
```

### Show (print)
```nat
show(x).
show("Hello " and name and "!").
show(x + y).
show(PI * r * r).
```

### Math  `+ - * / %`
```nat
show(10 + 3).     # 13
show(10 - 3).     # 7
show(10 * 3).     # 30
show(10 / 4).     # 2.5
show(10 % 3).     # 1
show((x + y) * 2).
```

### String concatenation  `and`
```nat
show("Hello " and name and "!").
show("Result: " and x + y).
```

### add … with … to …
English-style arithmetic assignment — add two values and store result:
```nat
add x with y to total.
add 100 with 200 to sum.
add name with "!" to name.
```
Equivalent to `total = x + y` in other languages.

### Functions
```nat
make greet with name inside:
    show("Hello " and name).
end.
greet("World").
```

Return a value with `give`:
```nat
make square with n inside:
    give n * n.
end.
show(square(7)).
```

Multiple parameters:
```nat
make add with a b inside:
    give a + b.
end.
show(add(5, 10)).
```

Recursive functions work:
```nat
make fib with n inside:
    if n < 2:
        give n.
    end.
    give fib(n - 1) + fib(n - 2).
end.
show(fib(10)).
```

### For loop  (INCLUSIVE — `from 1 to 5` prints 1 2 3 4 5)
```nat
repeat i from 1 to 5 step 1:
    show(i).
end.

# Step 2:
repeat i from 0 to 10 step 2:
    show(i).
end.

# Variable bounds:
fix START 1
fix END 10
repeat i from START to END step 1:
    show(i).
end.
```

### Repeat N times
```nat
repeat 5 times:
    show("Hi").
end.

let n be num(3).
repeat n times:
    show("loop").
end.
```

### While loop
```nat
let i be num(0).
while i < 10:
    show(i).
    let i be i + 1.
end.
```

### If / Else
```nat
if x > 5:
    show("big").
else:
    show("small").
end.
```

Operators: `==  !=  >  <  >=  <=`

English comparisons:
```nat
if x is greater than 5:  show("big").  end.
if x is less than 10:    show("ok").   end.
if x is not 0:           show("yes").  end.
```

Logical NOT:
```nat
if not x == 0:
    show("x is non-zero").
end.
```

### Arrays
```nat
let nums are 10 20 30 40 50.
show(nums[0]).
show(nums[4]).
```

### Input
```nat
show("What is your name?").
ask for name.
show("Hello " and name).

show("Enter two numbers:").
ask for a b.
add a with b to total.
show(total).
```

### Comments
```nat
# This is a comment
```

---

## Complete Example

```nat
fix MAX 10

make power with base exp inside:
    let result be num(1).
    repeat i from 1 to exp step 1:
        let result be result * base.
    end.
    give result.
end.

repeat i from 1 to MAX step 1:
    show(power(2, i)).
end.
```

---

## File Structure

| File           | Role                                      |
|----------------|-------------------------------------------|
| `nat.h`        | Types, constants, global state externs    |
| `tokenizer.c`  | Source text → flat token array            |
| `parser.c`     | Tokens → AST (recursive descent)          |
| `eval.c`       | Evaluate expression nodes → value         |
| `exec.c`       | Execute statement nodes, manage scope     |
| `main.c`       | Entry point, `fix` pre-pass, global state |
| `hello.nat`    | Full feature showcase                     |
| `examples.nat` | Fibonacci, factorial, power, more         |
