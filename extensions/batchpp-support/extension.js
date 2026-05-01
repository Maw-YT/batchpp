const vscode = require("vscode");
const path = require("path");

function activate(context) {
  const disposable = vscode.commands.registerCommand(
    "batchpp.compileCurrentFile",
    async () => {
      const editor = vscode.window.activeTextEditor;
      if (!editor) {
        vscode.window.showErrorMessage("No active editor.");
        return;
      }

      const inputPath = editor.document.uri.fsPath;
      const lowerInput = inputPath.toLowerCase();
      const isBatpp = lowerInput.endsWith(".batpp");
      const isCmdpp = lowerInput.endsWith(".cmdpp");
      if (!isBatpp && !isCmdpp) {
        vscode.window.showErrorMessage("Current file is not a .batpp or .cmdpp file.");
        return;
      }

      await editor.document.save();
      const outputPath = isCmdpp
        ? inputPath.replace(/\.cmdpp$/i, ".cmd")
        : inputPath.replace(/\.batpp$/i, ".bat");
      const command = `batppc "${inputPath}" "${outputPath}"`;

      const terminal = vscode.window.createTerminal("Batch++ Compiler");
      terminal.show(true);
      terminal.sendText(command);
      vscode.window.showInformationMessage(
        `Compiling ${path.basename(inputPath)} -> ${path.basename(outputPath)}`
      );
    }
  );

  context.subscriptions.push(disposable);
}

function deactivate() {}

module.exports = {
  activate,
  deactivate
};
