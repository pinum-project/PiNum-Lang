<p align="center">
  <img src="extras/assets/pinum_logo.svg" alt="PiNum Logo" width="200" />
</p>

# PiNum-Lang 1.3.0-beta2

https://pinum-project.github.io - official site for pinum. check for documentation and other info.
PiNum (represented by the `.pn` extension) is a lightweight, C-inspired programming language currently under development.
## ✨ Features

- **C-Style Syntax:** Familiar data types (`int`, `float`, `double`, `char`, `string`, `bool`) and control structures (`if`, `else`, `return`, `while`, `for`).
- **Type Modifiers:** `long`, `short`, `unsigned`, `signed`, and `long double` are supported on the appropriate base types.
- **Rich Expressions:** Arithmetic (`+ - * / %`), unary minus and logical not (`!`), comparisons and logical operators (`> < == != && ||`), and a ternary operator (`a > b ? a : b`).
- **Compound Assignment & Increment:** `+= -= *= /= %=` plus `++` and `--`.
- **Loop Sugar:** Three-part `for` loops plus a `for (N)` range form that runs the body N times with a hidden counter.
- **Vectors:** A growable, monomorphized `vec<T>` container with `append`, `size`, and indexed read/write.
- **Functions:** First-class `fn` definitions with optional return types and recursion.
- **Runtime Library Helpers:** Char repetition (`'c' * n`) and string concatenation (`"a" + "b"`).
- **Standard Library Support:** Includes a modular system using the `@import` directive (e.g., `@import stdlib`, `@import math`).
- **Editor Support:** Built-in syntax highlighting for:
  - **VS Code:** Extension available in `extras/vscode/`.
  - **Neovim:** Lua/Vim syntax files in `extras/nvim/`. To enable it, run the setup script:
    ```bash
    chmod +x activate_syntax.sh
    ./activate_syntax.sh
    ```

## 🚀 Getting Started

### One-line Installation

The fastest way to install PiNum-Lang on your system:

#### Linux/macOS (bash/zsh)
```bash
curl -sSL https://raw.githubusercontent.com/pinum-project/PiNum-Lang/main/install.sh | bash
```
*This will build the language and ask if you want to activate Neovim syntax highlighting and install the VS Code extension automatically.*

> **Windows users:** PiNum-Lang supports Linux/macOS only. If you're on Windows, use [WSL (Windows Subsystem for Linux)](https://learn.microsoft.com/en-us/windows/wsl/install) and follow the Linux instructions above.

### Prerequisites

- A C compiler (e.g., `gcc`) and `make`.

### Building from Source

To build the `pinum` binary:

```bash
make
```

The compiled binary will be located in the `bin/` directory.

### Installation

To install `pinum` to your system path:

#### Linux/macOS
```bash
sudo make install
```
This moves the binary to `/usr/local/bin/`.

### Running a Program

To transpile and compile a `.pn` file to a binary (default output is `a.out`):

```bash
./bin/pinum example/demonstration.pn
```

PiNum transpiles your code to C, then compiles it with an available C compiler (it searches for `cc`, `gcc`, `clang`, or `tcc`, or uses your `$CC`). The compiled binary lands in your current working directory.

### Output Flags

Control what gets produced with the output flags:

```bash
# compile to a binary named `program` (temporary C file is deleted)
./bin/pinum -o program example/demonstration.pn

# output only the C source file
./bin/pinum -o program.c example/demonstration.pn

# output both the C source file and a compiled binary
./bin/pinum -oc program example/demonstration.pn
```

Long forms `--output` and `--output-c` are also accepted. Use `--help` to see all flags.

## 📝 Syntax

Even though its under development, the syntax (to some extent) is defined.

**Semicolons are optional.** A newline ends a statement, but semicolons let you write multiple statements on one line:

```pinum
# both of these work
int a = 5;
int b = 5

# semicolons are needed to pack statements on one line
int c = 1; print(c, "\n"); c = c + 1; print(c, "\n");
```

### Comments
```pinum
# this is a comment!
# there is only single line comments.
```

### Libreries
```pinum
# stdlib is imported automatically, no need for @import
# this flag enables this language to be transpiled into bare C
@for engine
```

### Variable declaration
```pinum
# it has keywords like long, short, unsigned and signed

# declares an integer variable
int number = 10
# declares a floating point variable
float num = 3.14
# declares a double type variable
double numero = 2.718
# declares a character variable
char letter = 'P'
# declares a string variable
string name = "pinum"
# declares a boolean variable
bool ready = true

# type modifiers
unsigned int big = 4000000000
long int li = 123456789
short int sh = -30000
long double ld = 0.123456789
signed char sc = -5

# compound assignment operators
int amount = 10
amount += 5       # same as amount = amount + 5
amount -= 3
amount *= 2
amount /= 3
amount %= 4

# increment / decrement
amount++
amount--
```

### Output & input
```pinum
# print accepts any number of arguments; println adds a newline
print("hello ", name, "\n")
println("count = ", number)

# read stores user input into an existing variable
int data
read(data)
print(data, "\n")

# multiple statements on one line (requires semicolons)
int n = 2; n = n + 3; print(n, "\n")

# char repetition: 'c' * n repeats the char n times
println('=' * 30)
println('*' * (number - 3))

# string concatenation: "a" + "b" joins two strings
println("Hello, " + name + "!")
```

### Expressions
```pinum
int a = 10
int b = 3

# arithmetic and unary
println("a + b = ", a + b)
println("a % b = ", a % b)
println("neg = ", -a)
println("!flag = ", !false)

# comparisons and logicals
println("a > b = ", a > b)
println("a > b && b > 0 = ", a > b && b > 0)

# ternary
int max = a > b ? a : b

# expressions in assignment
int result = (a + b) * 2
```

### Conditions
```pinum
if (condition) {
        # task 1
} else if (condition) {
        # task 2
} else {
        # default task
}
```

### Loops
```pinum
# while loop
while (condition) {
        # task
        if (something) {
                break;      # exit the loop
        }
        if (something_else) {
                continue;   # skip to the next iteration
        }
}

# three-part for loop
for (int f = 0; f < 5; f++) {
        println("f = ", f)
}

# counted loop with a step
for (int step = 0; step < 10; step += 2) {
        println("step = ", step)
}

# for (N) range sugar: runs the body N times with a hidden counter
for (5) {
        println("beep")
}

# nested range loops each get a unique hidden counter
for (3) {
        for (2) {
                print("*")
        }
        println()
}
```

### Vectors
```pinum
# a growable vector, monomorphized per element type
vec<int> nums = [3, 5, 6]
vec<float> ratios = [1.5, 2.5]
vec<string> words = ["PiNum", "rocks"]
vec<int> empty = []

# methods and properties
nums.append(7)
println("nums has ", nums.size, " elements")

# element access (read and write)
println("nums[0] = ", nums[0])
nums[1] = 99
```

### Functions
```pinum
# define with: fn name(params): returnType { ... }
# the ': returnType' part is optional; omit it for a void function.
fn square(int n): int {
        return n * n
}
fn greet() {
        println("Hello from a function!")
}

# recursion
fn factorial(int n): int {
        if (n <= 1) {
                return 1
        }
        return n * factorial(n - 1)
}
```

### Return
```pinum
# exits the program (or current function) with the given value
return 0
```

## 🛠 Project Structure

- `src/`: Core implementation (Lexer, Parser, Codegen, Helper functions).
- `include/`: Header files defining the language structures and interfaces.
- `bin/`: Compiled binaries.
- `example/`: Sample programs demonstrating language features.
- `extras/`: Editor extensions and syntax highlighting.
- `test/`: Automated tests for the lexer, parser, AST, and codegen.

## 🧪 Testing

Run the test suite interactively (choose which category to run, optionally with Valgrind):

```bash
bash test/run_tests.sh
```

Tests cover the lexer, parser, AST, and codegen stages. Codegen tests transpile each program to a C file, compile it, and run the resulting binary to confirm it behaves correctly.

## 🚧 Development Status

PiNum is currently in its early stages:
- [x] Lexer / Tokenizer
- [x] AST
- [x] Parser
- [x] Code Generation (print, read, variables, type modifiers, arithmetic, comparisons, logicals, ternary, compound assignment, increment/decrement, if/else, while, for, break, continue, return)
- [x] Vectors (`vec<T>`) with methods and indexed access
- [x] Functions (definitions, return types, recursion)
- [x] Runtime Library (char repetition, string concatenation)
- [ ] object oriented programming, data structures (in progress)

## 🤝 Contributing

Contributions are what make the open-source community such an amazing place to learn, inspire, and create. Any contributions you make are **greatly appreciated**.

Please see our [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines on branching, coding standards, and testing.

## 📄 License

This project is licensed under the [MIT License](LICENSE).

I use Neovim BTW.
