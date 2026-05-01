# Batch++ Compiler (MVP)

`batppc` transpiles `.batpp` or `.cmdpp` source files into plain `.bat` or `.cmd` scripts.

## Features (current MVP)

- `import "file.batpp"` (recursive)
- `module name`
- `fn name(args) { ... }` with `return` (functions are emitted as `module__name__<arity>`; default arguments on trailing parameters)
- `export fn name(...) { }` — required for `othermod::name` calls from another module
- `macro fn` / `inline fn` — body expanded at call sites (no separate `:label`)
- Anonymous/local blocks `{ ... }` — emitted with `setlocal` / `endlocal` scope boundaries
- Bare function calls: `doThing()` or `mod::doThing(arg1, arg2)` (no `call :label` needed)
- Error handling:
  - `throw "message"` (or `throw expr`)
  - `try { ... } catch err { ... }` and optional `finally { ... }` (also `} finally {` after `catch`)
  - Catch object fields: `%err.code%`, `%err.message%`
- `assert <batch-condition>` and `assert <batch-condition>, "message"`
- Loops:
  - `while <batch-condition> { ... }` with `break` / `continue`
  - `for i = <from> to <to> { ... }` (numeric)
  - `for x in listVar { ... }` (`listVar` is a token list variable)
  - `for x in arr name { ... }` — walks indices `0 .. len-1`, sets `x` from `__arr_name_*`
- Control flow: `if <batch-condition> { ... }`, optional chains `else if <batch-condition> { ... }`, then optional `else { ... }` (each keyword starts its own line after the closing `}` of the previous branch)
- `match <discriminator> { case "lit": { ... } default: { ... } }` (discriminator is usually `%var%` or a literal expression)
- Variables: `let`, `var`, `const` declarations
- Variable arithmetic updates: `x += 2`, `x -= 1`, `x *= 3`, `x /= 2`
- Variable increment/decrement: `x++`, `x--`
- String interpolation: `${name}`, `${arr[0]}`, `${this.field}`, `${len(arr)}`
- Block comments `/* ... */` (non-nesting); line pragmas `#encoding ...`, `#!batpp`, `#pause-on` (append `pause` before `endlocal`), `#pause-off` (suppress that pause); if both appear across imports, `#pause-off` wins
- Multiline / raw strings: `""" ... """` (lines merged in preprocessing)
- `arr name = [a, b, c]`
- `name.push(value)`
- `%arrName[index]%` reads; `name[index] = value`; `slice dest = src, start, count`; `len(name)` in expressions
- `map m = { "key": value, ... }` — keys stored as `__map_m_<key>` and `__map_m_keys`
- `struct Name { var field = default; ... }` and `st inst = Name()` — fields `__st_inst_field`
- `enum E { A, B }` — emits `set "E_A=0"`, `E_B=1`, ...
- String helpers: `let x = str_trim(var)`, `let x = str_contains(var, "sub")`, `let x = str_replace(var, "from", "to")`
- Input helpers:
  - `input line` (plain stdin read)
  - `input line "Prompt: "` (prompted read)
  - `input? line` (EOF-safe read; sets `__input_ok` to `1` or `0`)
  - `input? line "Prompt: "` (same with prompt)
- UCI parsing helpers:
  - `token cmd = line, 1` (extract Nth space-delimited token)
  - `after moves = line, " moves "` (substring after marker; empty if missing)
  - `startswith isGo = line, "go"` (case-insensitive prefix check; sets `1` or `0`)
- `class Name { var field; fn method() { ... } }`
- `obj x = new Class(args)`
- `x.method(args)`
- `this.field = ...` and `%this.field%` inside methods

## Build

```powershell
cmake -S . -B build
cmake --build build --config Release
```

## Compile

```powershell
.\build\batppc.exe ".\Uci Engine\uci_engine.batpp" ".\Uci Engine\uci_engine.bat"
.\build\batppc.exe ".\some_script.cmdpp" ".\some_script.cmd"
.\build\batppc.exe --debug-echo --emit-header ".\script.batpp" ".\script.bat"
```

Then run:

```powershell
cmd /c ".\Uci Engine\uci_engine.bat < .\Uci Engine\uci_input_cmd_full.txt"
```

## UCI Engine Example

A toy UCI chess engine source is included:

- Source: `Uci Engine/uci_engine.batpp`
- Generated batch: `Uci Engine/uci_engine.bat`

Compile it with:

```powershell
.\build\batppc.exe ".\Uci Engine\uci_engine.batpp" ".\Uci Engine\uci_engine.bat"
```

Current UCI commands handled:

- `uci`
- `isready`
- `ucinewgame`
- `position startpos moves ...`
- `go depth N`
- `stop`
- `quit`

Redirected test tip (Windows):

- Create test input as ASCII/ANSI text for `cmd.exe` parsing (for example: `Set-Content -Encoding ascii` in PowerShell, or `echo ... > file` via `cmd /c`).

## Tests

```powershell
.\build\batppc.exe ".\tests\syntax_all.batpp" ".\tests\syntax_all.bat"
cmd /c ".\tests\syntax_all.bat"
.\tests\run_negative_diagnostics.ps1
.\tests\run_parser_maintainability_checks.ps1
.\tests\run_golden_suite.ps1
```

## Formatting/style

- Batch++ source formatting conventions are documented in `docs/style_guide.md`.
- Language construct/limitation overview is in `docs/language_cheat_sheet.md`.

## Notes

- This is a practical MVP transpiler, not a fully strict parser yet.
- Arithmetic assignment compiles to `set /a` in batch.
- Function calls inside conditionals should be placed on their own line inside a block, e.g. `if ... ( doThing() )`.
- Braces in `fn` / `struct` / `match` / control blocks are tracked per-line (`{` / `}` counts); put `{` / `}` inside strings carefully.
- `input?` is useful for real-time protocols (like UCI) where piped stdin may end; check `%__input_ok%`.
- Lexical identifier/operator rules are centralized in `docs/lexical_rules.md`.
- UCI helper example:
  - `token cmd = line, 1`
  - `startswith isPos = line, "position"`
  - `after moveTail = line, " moves "`
