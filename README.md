# Batch++

Source code and issue tracker live on GitHub: [github.com/Maw-YT/batchpp](https://github.com/Maw-YT/batchpp).

## The story (super simple)

Imagine your computer is a helper that only understands **very old, picky instructions** called batch files. Those instructions work, but writing them by hand is like building a castle out of tiny blocks without a picture on the box.

**Batch++** is a friend that sits in the middle. **You** write nicer, clearer code in files that end with `.batpp` or `.cmdpp`. **Batch++** turns that into normal `.bat` or `.cmd` files the computer already knows how to run.

So: you write the easy story, Batch++ whispers the picky version to Windows for you.

## What is this project, really?

This folder holds a small program named **`batppc`** (think “batch plus plus compiler”). It is not magic dust; it is a **translator**. One language in, batch script out.

You can use it for:

- Little tools that fix files or folders  
- Games or toys that talk through a black window  
- Anything where you would normally write a long, twisty batch file but wish it looked more like a tiny program  

The big idea is **comfort**: loops, functions, errors you can catch, lists, maps, and other ideas that make long scripts easier to read than plain batch.

## What can Batch++ help you write?

You do **not** need to memorize all of this. Think of it as a toy box full of parts:

- **Pieces that fit together** — `import` pulls in other files; `module` gives them names.  
- **Named jobs** — `fn` and `export fn` are little recipes you can call again and again.  
- **Safety nets** — `try` / `catch` / `finally` and `throw` when something goes wrong.  
- **Repeating and choosing** — `while`, `for`, `if`, `match` so the script can think a bit.  
- **Boxes for data** — variables, arrays, maps, structs, enums, strings with `${...}` inside.  
- **Bigger toys** — classes and `new` if you want objects with methods.  
- **Talking and listening** — helpers for reading lines and prompts when your script needs to ask questions.  

If you want the grown-up list with every knob and switch, open **`docs/language_cheat_sheet.md`**. That is the long, exact version of the toy list.

## How do you “build” the translator?

“Build” means: turn the C++ source in this folder into a real **`batppc.exe`** you can double-click or run from a terminal.

In PowerShell, from this project folder:

```powershell
cmake -S . -B build
cmake --build build --config Release
```

When that finishes without yelling errors, you have your translator.

## The smallest example

Make a file named **`hello.batpp`** next to your project (or anywhere you like) with this inside. Save it as **UTF-8 without a BOM** (or plain ASCII) so the first line does not get a hidden character that confuses `cmd.exe`.

```text
module hello

fn main() {
  let x = 42
  return 0
}

main()
```

Then turn it into a batch file and run it:

```powershell
.\build\batppc.exe ".\hello.batpp" ".\hello.bat"
cmd /c ".\hello.bat"
```

So: **Batch++ writes `hello.bat`**; **Windows runs `hello.bat`**. Change the paths if your files live somewhere else.

## More reading (when you want details, not stories)

- **`docs/style_guide.md`** — how we like Batch++ source to look.  
- **`docs/language_cheat_sheet.md`** — all the language bits in one place.  
- **`docs/lexical_rules.md`** — the picky spelling rules for names and symbols.  

## Honest small print (still simple)

- Batch++ is a **useful work-in-progress**, not a perfect robot that catches every mistake.  
- Some things work best if you put function calls on their own line inside `if` blocks—see the cheat sheet for the picky cases.  
- Curly braces `{` `}` in your code are counted carefully; do not hide them inside strings in sneaky ways.  

Have fun. Start small, run the `.bat` you made, and grow your scripts a little at a time.
