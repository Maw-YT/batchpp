# Batch++ lexical rules

This document is the single source of truth for parser-facing lexical rules used by `batppc`.

## Identifier rules

- First character: `A-Z`, `a-z`, or `_`
- Remaining characters: `A-Z`, `a-z`, `0-9`, or `_`
- Canonical validator in code: `isValidIdentifier()` in `src/compiler_utils.cpp`

Examples:

- Valid: `main`, `_tmp`, `value_2`, `mod_name`
- Invalid: `2fast`, `has-dash`, `with space`

## Assignment operator rules

Supported arithmetic assignment operators are exactly:

- `+=`
- `-=`
- `*=`
- `/=`

Canonical validator in code: `isSupportedAssignmentOperator()` in `src/compiler_utils.cpp`.

## Tokenization layer

Batch++ now layers a lightweight tokenizer in front of parser paths that previously depended only on line-wide regex:

- Tokenizer entrypoint: `tokenizeLine()` in `src/compiler_utils.cpp`
- Parser helpers:
  - `tryParseKeywordHeader()`
  - `tryParseVarDeclLine()`
  - `tryParseFunctionHeader()` (now token-driven for identifier/header shape)

This tokenizer layer is intentionally small and pragmatic; full grammar parsing can expand from this foundation without duplicating lexical rules.
