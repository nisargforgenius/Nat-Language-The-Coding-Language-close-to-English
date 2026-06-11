# NAT Language v3.6

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

## What's New in v3.6

Full File I/O system — read, write, append, insert, remove, and loop over files.

- `write "text" to "file.txt".` — create or overwrite
- `write "text" to "file.txt" at line N.` — overwrite a specific line
- `append "text" to "file.txt".` — add to end
- `insert "text" to "file.txt" at line N.` — insert, pushes lines down
- `remove line N from "file.txt".` — delete a specific line
- `read "file.txt".` — print file with `line N | content` format
- `let x be read "file.txt" at line N.` — read a line into a variable
- `if file "file.txt" exists:` — check if a file exists
- `delete file "file.txt".` — delete a file
- `each line in file "file.txt":` — loop over every line
- `write x inside "file.nat"` — runtime variable injection (no disk write)
- `x = 10.` — `=` now works as assignment alongside `x be 10.`
- Bare function calls auto-print return value — `area_rect(2,5).` prints without needing `show`

### Bug fixes
- `sum = num1 + num2.` was incorrectly treated as a function call — fixed
- `area_rect(2,5).` as a bare statement returned 0 instead of the result — fixed

> **Note:** Windows users should use v3.6.1 instead — it includes a CRLF hotfix.

---

## Language Reference

### Variables
```nat
let x be 42.
let name be "Nisarg".
let x, y, z.

// = also works as assignment
x = 100.
```

### File I/O
```nat
// Write
write "Hello NAT" to "notes.txt".
append "Another line" to "notes.txt".
write "Updated" to "notes.txt" at line 1.
insert "New first line" to "notes.txt" at line 1.
remove line 2 from "notes.txt".

// Read
read "notes.txt".
let line1 be read "notes.txt" at line 1.
show line1.

// Check and delete
if file "notes.txt" exists:
    show "file found".
    delete file "notes.txt".
end.

// Loop over lines
each line in file "log.txt":
    show line.
end.
```

### Runtime injection
```nat
// write x inside injects x into the target file at runtime only
// the file on disk is NEVER modified
let x be 10.
write x inside "other.nat".
// when other.nat runs, x will be 10 regardless of what other.nat declares
```

### Using the standard library
```nat
use math.tree
show sqrt(144).
show pow(2, 10).

use geometry.tree
area_rect(5, 3).       // auto-prints: 15
show area_circle(7).

use string.tree
show capitalize("hello world").

use convert.tree
show celsius_to_fahrenheit(100).
```

### Arrays
```nat
let fruits are "apple" "mango" "banana".
each item in fruits:
    show item.
end.
```

### Functions
```nat
make add with a b inside:
    give a + b.
end.

add(3, 4).        // auto-prints: 7
show add(10, 20). // also works
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

### Create graph
```nat
create graph "Sales":
    type bar.
    label "Jan" value 10.
    label "Feb" value 25.
    label "Mar" value 40.
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
// Write a log file and read it back

write "NAT v3.6 launched." to "log.txt".
append "File I/O working." to "log.txt".
append "All systems go." to "log.txt".

show "--- Log contents ---".
read "log.txt".

show "--- Processing each line ---".
each line in file "log.txt":
    show ">> " and line.
end.

delete file "log.txt".
show "Log cleared.".
```

---

## File Structure

```
nat_language_v3.6/
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
├── examples/
│   ├── hello.nat
│   ├── examples.nat
│   ├── fileio_test.nat
│   └── graph_test.nat
└── README.md
```
