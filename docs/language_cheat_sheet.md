# Batch++ language cheat sheet

One-page summary of supported constructs and current limitations.

## Modules and imports

### Supported
- `module name`
- `import "relative_file.batpp"` (recursive)
- `export fn name(...) { ... }` for cross-module calls

### Limitations
- Imports are path-based; no package manager/module registry.
- Cross-module visibility is function-level (`export fn`), no namespace aliasing.

## Functions

### Supported
- `fn name(args) { ... }`
- `return expr`
- trailing default parameters
- inline expansion: `macro fn` / `inline fn`
- overload resolution by arity/default-fill behavior

### Limitations
- No typed signatures/type checking.
- No lambdas/closures.

## Variables and expressions

### Supported
- declarations: `let`, `var`, `const`
- arithmetic update operators: `+=`, `-=`, `*=`, `/=`
- increment/decrement: `x++`, `x--`
- interpolation: `${name}`, `${arr[0]}`, `${this.field}`, `${len(arr)}`

### Limitations
- Expression parsing is pragmatic, not a full precedence-aware language parser.

## Control flow

### Supported
- `if ... { ... } else if ... { ... } else { ... }`
- `while ... { ... }` with `break` / `continue`
- `for i = a to b { ... }`
- `for x in listVar { ... }`
- `for x in arr arrName { ... }`
- `match value { case "x": { ... } default: { ... } }`
- anonymous/local scope blocks: `{ ... }` (`setlocal/endlocal` boundaries)

### Limitations
- Conditions map to batch condition style and semantics.
- Loop constructs compile to helper labels (not native high-level runtime structures).

## Errors and safety

### Supported
- `throw expr`
- `try { ... } catch err { ... }`
- optional `finally { ... }`
- `assert condition`
- diagnostics include file/line/source snippet on failures
- cyclic import detection
- warning on unknown `mod::fn` references (best effort)

### Limitations
- No typed exception hierarchy.
- Warning checks are static best effort, not full semantic analysis.

## Collections and data

### Supported
- arrays: `arr name = [a, b]`, `name.push(v)`, `name[i] = v`, `%name[i]%`, `slice`, `len(name)`
- nested array literals and static nested indexing: `arr g = [[a,b],[c,d]]`, `%g[1][0]%`, `g[1][0] = z`
- maps: `map m = { "k": v }`
- structs: `struct S { var f = v }` and `st x = S()`
- enums: `enum E { A, B }`

### Limitations
- Data structures compile down to naming conventions over batch variables.
- No deep structural typing or runtime reflection.

## OOP-lite

### Supported
- `class Name { var field; fn method() { ... } }`
- `obj x = new Class(args)`
- `x.method(args)`
- `this.field` set/read in methods

### Limitations
- No inheritance/interfaces.
- Dynamic dispatch is limited to generated label patterns.

## Strings, comments, pragmas

### Supported
- normal strings: `"text"`
- raw/multiline strings: `""" ... """`
- block comments: `/* ... */`
- pragmas: `#!batpp`, `#encoding`, `#pause-on`, `#pause-off`
- string helpers: `str_trim`, `str_replace`, `str_contains`

### Limitations
- Escape/quoting semantics ultimately constrained by generated batch behavior.

## I/O helpers

### Supported
- `input line`
- `input line "Prompt: "`
- `input? line` (EOF-safe with `__input_ok`)
- token helpers for protocol parsing: `token`, `after`, `startswith`

### Limitations
- Input model follows `cmd` constraints and redirection behavior.

## Tooling quick reference

- Syntax/style: `docs/style_guide.md`
- Lexical spec: `docs/lexical_rules.md`
- Negative diagnostics suite: `tests/run_negative_diagnostics.ps1`
- Parser maintainability suite: `tests/run_parser_maintainability_checks.ps1`
- Golden snippets suite: `tests/run_golden_suite.ps1`
