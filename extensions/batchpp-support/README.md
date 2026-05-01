# Batch++ Support

This extension adds basic Batch++ (`.batpp` / `.cmdpp`) support for both VS Code and Cursor.

## Included

- `.batpp` and `.cmdpp` file recognition
- Syntax highlighting for Batch++ keywords, strings, directives, and variables
- Bracket/comment configuration
- Starter snippets (`fn`, `main`, `if`, `try`)
- Command: **Batch++: Compile Current File**

## Compile command

The command runs:

```powershell
batppc "<current-file>.batpp" "<current-file>.bat"
batppc "<current-file>.cmdpp" "<current-file>.cmd"
```

Make sure `batppc` is available in your PATH (or invoke VS Code/Cursor from a shell where it is available).

## Install (VS Code and Cursor)

1. Open the `extensions/batchpp-support` folder in a terminal.
2. Install packaging tool if needed:

   ```powershell
   npm install -g @vscode/vsce
   ```

3. Package extension:

   ```powershell
   vsce package
   ```

4. Install generated `.vsix`:
   - VS Code: `Extensions` -> `...` -> `Install from VSIX...`
   - Cursor: `Extensions` -> `...` -> `Install from VSIX...`

## Development

Open this extension folder in VS Code and press `F5` to launch an Extension Development Host.
