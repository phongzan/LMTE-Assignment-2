# Flux language interpreter

Flux is a C++17 tree-walking interpreter for a dynamically typed language with
built-in variable history. This README gives the complete installation,
operation and testing procedure expected by the assessment rubric.

## 1. Prerequisites

Install:

- CMake 3.16 or newer;
- a C++17 compiler;
- on Windows, Visual Studio 2022 Build Tools with **Desktop development with
  C++** and a Windows SDK.

No third-party C++ libraries are required.

## 2. Configure and build

Open a terminal in the repository root—the directory containing
`CMakeLists.txt`—and run:

```powershell
cmake -S . -B build
cmake --build build --config Debug
```

On Windows the executable is `build/Debug/flux.exe`. With a single-configuration
generator it may instead be `build/flux`.

### VS Code

The repository contains `.vscode/settings.json` and `.vscode/tasks.json`.

1. Open the repository folder in VS Code.
2. Select a C++ compiler kit if CMake Tools asks for one.
3. Press `Ctrl+Shift+B` to run the default build task.
4. Run **Tasks: Run Task > CMake: Test** to execute every test.

## 3. Run a Flux program

Windows:

```powershell
./build/Debug/flux.exe examples/stock_ledger.flx
```

macOS/Linux or a single-configuration build:

```bash
./build/flux examples/stock_ledger.flx
```

Expected stock example output:

```text
45
[50, 45]
```

Use memory instrumentation with:

```powershell
./build/Debug/flux.exe --mem tests/pass/p22_memory.flx
```

The CLI returns a non-zero exit code for lexical, syntax, semantic or runtime
errors and writes the diagnostic to standard error.

## 4. Language syntax

Each statement occupies one logical line, braces enclose blocks and `#` begins a
comment.

```text
42 -> quantity                 # mutable variable
0.1 => GST                    # constant
quantity + 5 -> quantity      # arithmetic update

when quantity < 10 {
    show "low"
} otherwise {
    show "ok"
}

repeat 4 as i { show i }
until quantity > 100 { quantity * 2 -> quantity }
each item in [1, 2, 3] { show item }

fn double(value) { give value * 2 }
5 then double then show
```

### Arrays

Arrays are zero-based, may be nested and may contain mixed types.

```text
[[1, 2], [3, 4]] -> matrix
99 -> matrix[0][1]
push(matrix, [5, 6]) -> matrix
show len(matrix)
```

### History and rewind

```text
10 -> stock
20 -> stock
show stock~1          # 10
show history(stock)  # [10, 20]
rewind stock          # current value becomes 10
```

### Scope and closures

Function writes are local unless marked `outer`. Nested functions retain their
declaration environment, which provides closure behaviour.

## 5. Repository structure

```text
include/flux/       Public tokens, AST and component interfaces
src/                Lexer, parser, semantic analyser, interpreter and runtime
tests/              Component, interpreter and golden-file tests
tests/pass/         Valid `.flx` programs and exact `.expected` output
tests/fail/         Invalid `.flx` programs and expected diagnostics
examples/           Programs that can be run directly
.github/workflows/  Continuous-integration configuration
main.cpp             Command-line compiler/interpreter pipeline
```

## 6. Run all provided tests

```powershell
ctest --test-dir build -C Debug --output-on-failure
```

This runs the lexer/parser, semantic, runtime, interpreter and CLI golden-file
suites. A feature is complete only when CTest reports `100% tests passed`.

## 7. Add or modify a bespoke test

### Fast component/interpreter test

Add a case to the relevant file:

- `tests/frontend_tests.cpp` for token or AST behaviour;
- `tests/semantic_tests.cpp` for pre-execution rules;
- `tests/runtime_tests.cpp` for `Value`, `Environment` or operators;
- `tests/interpreter_tests.cpp` for a complete in-memory Flux program.

Rebuild and run CTest after changing a C++ test.

### End-to-end golden test

1. Copy an existing program in `tests/pass` or `tests/fail`.
2. Edit the `.flx` source using the syntax above.
3. Edit the matching `.expected` file. Successful tests compare standard output;
   failing tests compare standard error and require a non-zero exit code.
4. Register the pair with `add_flux_golden_test(...)` in `CMakeLists.txt`.
5. Reconfigure, build and run CTest.

The CMake runner normalizes Windows and Unix line endings before performing an
exact comparison.

## 8. Implemented assessment features

- tokenisation and lexical analysis;
- recursive-descent syntax analysis and panic-mode recovery;
- semantic analysis and dynamic runtime type checking;
- variables, constants and literals;
- arithmetic, comparison and Boolean logic;
- selection, three forms of iteration, functions and recursion;
- nested arrays and scope chains;
- closures and reference-counted dynamic memory;
- domain-specific variable history, rewind and pipelines;
- automated component, integration and CLI tests.

Flux is an interpreter, so its final pipeline stage executes the AST directly
instead of emitting machine code. This is the interpreter alternative specified
by the assignment.
