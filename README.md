# NAT (Natural Abstract Tree) Language v3.6.1

A simple, English-style compiled programming language written in pure C.
Designed to read like natural English — no symbols where words will do.
This language was created by Nisarg with the help of Claude Sonet Model.
The earliest version V1.0 and V2.0 of NAT was written by me, but it has so many bugs,
that i had to take help of Sonet to make it stable. So it changed my code and edited it
to a better and stable form.

When I first started learning about coding, i was curious about how a coding langauge was created, how did Dennis Ritchie created C and why C performs better than other languages. This made me do Research in coding and at the mid-sem of my second year (yup, I am a college student), I thought of the idea of creating my own language which is simple to understand and is fun to write. I created the first version, the V1.0 and 2.0, had very basic tokenizer and parser and that too in one main.c file, which didnt allowed multi-line code and Idents were broken, this made me give up on the project but then I realised this is an era of AI, so why not use AI for Research and Development of this project, so I first asked ChatGPT about creating a langauge and it told me that its possible to create by taking source code of Python but I didnt want to use any open source project as this projects didnt have what I want, so I uploaded my broken main.c file to Claude and to my suprise, it understood the method, the code structure, my ideology and improved my main.c into seperate parser, token etc files and from there the first stable version of my Language was created "V3.0", it also become my coding partner. I code and It improves it.

I hope the devs will check this project.

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

## What's New in v3.6.1

Hotfix for Windows users on v3.6.

- **Windows CRLF fix** — files written on Windows were getting `\r\n` line endings injected by the OS. This caused `read "file.txt"` to display incorrectly in the terminal (line 1 appearing to vanish due to carriage return)
- All file operations now use **binary mode** — consistent behavior on both Windows and Linux
- Line reading strips both `\r` and `\n` — files written by Notepad or other Windows editors now read cleanly too

> **Windows users on v3.6 should upgrade to v3.6.1.**

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
let x be 10. //this code will set the value of another X i.e a different file.nat X value to 10
write x inside "other.nat". // this if executed will give output 10 
// other.nat runs with x = 10, file on disk unchanged
```

### Using the standard library
```nat
use math.tree
show sqrt(144).
show pow(2, 10).

use geometry.tree
area_rect(5, 3).
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
show add(10, 20).
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
write "NAT v3.6.1 launched." to "log.txt".
append "File I/O working." to "log.txt".
append "All systems go." to "log.txt".

show "--- Log contents ---".
read "log.txt".

each line in file "log.txt":
    show ">> " and line.
end.

delete file "log.txt".
show "Done.".
```

---

## File Structure

```
nat_language_v3.6.1/
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
