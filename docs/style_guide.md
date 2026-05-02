# Batch++ style guide

This is the recommended formatting/style for `.batpp` source examples and fixtures.

## Indentation and spacing

- Use **2 spaces** for indentation inside blocks.
- Keep one space around binary operators where readability benefits (`x += 1`, `a == b`).
- Do not align with tabs.

## Braces and blocks

- Put the opening brace on the same line for declarations/control flow:
  - `fn main() {`
  - `if cond {`
- Put the closing brace on its own line.
- For `else if` / `else`, place keyword on the next line after `}`:
  - `}`
  - `else if ... {`
  - `else {`

## Declarations

- Prefer descriptive snake/camel style names using valid identifier chars (`[A-Za-z_][A-Za-z0-9_]*`).
- Use `let` for local values in examples unless mutability is required.
- Keep one declaration per line.

## Modules and functions

- Put `module ...` near the top of file.
- Separate top-level declarations with a blank line.
- Keep `main()` at the end for runnable examples.

## Strings and literals

- Use double-quoted strings where possible.
- Keep raw multiline strings (`"""`) only when truly needed.

## Comments

- Use short `//` comments directly above the code they describe.
- Keep comments focused on intent, not syntax.
