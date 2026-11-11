# TPC Compiler

A compiler for TPC, a small C-like language, written in C with Flex and Bison. It checks a program and generates NASM x86-64 assembly.

Individual project for the third-year compilation course at Université Gustave Eiffel. The report is in French.

## Pipeline

1. **Lexical analysis** (`src/tpc_lexer.l`, Flex)
2. **Parsing** into an abstract syntax tree (`src/tpc_parser.y`, Bison; `src/tree.c`)
3. **Semantic analysis** with symbol tables for globals, functions, parameters and locals (`src/symbol.c`)
4. **Code generation** to NASM x86-64 (`src/nasm.c`), with built-in I/O routines in `builtin/` (`getchar`, `getint`, `putchar`, `putint`)

The language has `int` and `char` types, arrays, functions, `if`/`else`, `while`, and arithmetic, comparison and boolean operators.

## Build

Requires `gcc`, `flex`, `bison` and `make`. `nasm` and `ld` are needed to assemble the generated code.

```sh
cd ProjetCompilationL3_Yunliang_CAO
make
```

The compiler is built as `bin/tpcc`.

## Usage

```sh
./bin/tpcc < program.tpc     # writes _anonymous.asm
./bin/tpcc program.tpc       # writes nasm/program.asm
./bin/tpcc -t < program.tpc  # also print the syntax tree
```

| Option | Meaning |
| --- | --- |
| `-t`, `--tree` | Print the abstract syntax tree |
| `-s` | Print the symbol tables |
| `-d`, `--debug` | Parser debug mode |
| `-h`, `--help` | Help |

Exit codes: `0` for a valid program (warnings allowed), `1` for a syntax error, `2` for a semantic error.

## Tests

```sh
make test                  # run every test and write rep/result.txt
make TEST_FLAG=-t test     # same, printing syntax trees
```

The suite has 196 programs. The last recorded run, in `rep/result.txt`, passes all of them:

| Folder | Programs | Expected |
| --- | --- | --- |
| `test/good` | 82 | Compiles |
| `test/warn` | 14 | Compiles with warnings |
| `test/syn-err` | 41 | Syntax error |
| `test/sem-err` | 59 | Semantic error |

## Example

```c
int double(int x) {
    return x * 2;
}

int main(void) {
    int y;
    y = double(21);
    putint(y); // 42
    return 0;
}
```
