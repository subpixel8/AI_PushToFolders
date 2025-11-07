# PushToFolders

PushToFolders is a tiny Windows command line utility that sorts files into folders that share their names. Each file is moved into a sibling folder whose name matches the file name (without the extension).

The executable supports two usage scenarios:

1. **Command line** — supply a single folder path and every file inside it is moved into a like-named folder.
2. **Windows File Explorer context menu** — select one or more files, invoke the tool, and each selected file is moved into its own folder.

All errors are written to a persistent log file so that you always have an audit trail of what happened.

## Building a standalone executable on Windows

The project contains a single C++ source file that targets C++17 and only depends on the standard library.

1. **Install Visual Studio 2022**
   * Download [Visual Studio Community 2022](https://visualstudio.microsoft.com/) and run the installer.
   * When prompted for workloads, tick **Desktop development with C++** and complete the installation using the default options. This installs the MSVC compiler (`cl.exe`).

2. **Open the Developer Command Prompt**
   * Use the Start menu to search for **Developer Command Prompt for VS 2022** and launch it. The prompt configures the environment so that the compiler and Windows SDK tools are available.

3. **Compile the program**
   * Copy the repository folder to a convenient location if you have not already done so (for example `C:\tools\PushToFolders`).
   ```cmd
   cd C:\tools\PushToFolders
   cl /std:c++17 /EHsc /O2 /W4 /MT /Fe:PushToFolders.exe src\main.cpp /link /SUBSYSTEM:WINDOWS shell32.lib
   ```

   The `/MT` switch links the static Microsoft C++ runtime so that `PushToFolders.exe` is fully self-contained and does not require separate redistributable packages.

   Adding `/SUBSYSTEM:WINDOWS` prevents an extra console window from appearing when the tool is launched from File Explorer while still allowing it to reuse the console that invoked it from the command line. Linking against `shell32.lib` provides the Windows implementation of `CommandLineToArgvW`, which the program uses to parse Explorer-launched invocations.

4. **Copy the executable** (`PushToFolders.exe`) to a folder that is easy to reference, for example `C:\Program Files\PushToFolders`. You can safely delete the source files if you only need the executable afterwards.

## Using the executable

Open *Command Prompt* and run one of the commands below. Always wrap paths that contain spaces in double quotes.

```cmd
PushToFolders "C:\Users\you\Pictures"
PushToFolders "C:\Users\you\Documents\Report.docx" "D:\Archives\Budget.xlsx"
PushToFolders --show-log
PushToFolders --clear-log
```

> **Important:** The program fully supports paths that contain characters such as ampersands (`&`), emoji, or characters from other languages. The Windows command interpreter treats `&` as a command separator, so if you type a command manually and the path contains `&`, escape it as `^&` (for example, `"C:\Games^&Art\cover.txt"`). No extra steps are required when launching the tool from File Explorer.

* When you pass exactly one argument and it is a folder, the program scans it for regular files.
* When you pass one or more file paths, each file is moved into a folder named after the file.
* `--show-log` prints the error log, and `--clear-log` erases the log file so that the next run starts fresh. You can pass both switches at the same time to review the current log before clearing it.

Successful operations are echoed to the console, while errors are written both to the console (when available) and to the log file.

> **Tip:** When the tool is started from File Explorer it does not display a console window. Run `PushToFolders --show-log` later to review the log or use the command line directly if you want to watch progress in real time.

### Log file location

The log file is stored inside your `%LOCALAPPDATA%\PushToFolders` folder. If that folder cannot be created, the program falls back to the system temporary directory.

## Adding the Windows File Explorer context menu entry

The context menu is configured with a small registry script. The script adds a **"Push files into folders"** option when you right-click any file.

1. Open *Notepad*.
2. Paste the snippet below and adjust the path so that it points to the actual location of `PushToFolders.exe`.

   ```reg
   Windows Registry Editor Version 5.00

   [HKEY_CURRENT_USER\Software\Classes\*\Shell\PushToFolders]
   @="Push files into folders"
   "Icon"="\"C:\\Program Files\\PushToFolders\\PushToFolders.exe\""
   "MultiSelectModel"="Document"

   [HKEY_CURRENT_USER\Software\Classes\*\Shell\PushToFolders\Command]
   @="\"C:\\Program Files\\PushToFolders\\PushToFolders.exe\" \"%1\""
   ```

3. Save the file as `PushToFolders_ContextMenu.reg`.
4. Double-click the saved file and confirm the prompts to add it to the registry. Ensure the key names stay capitalised exactly as shown (`Shell` and `Command`) when editing the file manually.

The command above registers the handler for every file type for the current user. The `MultiSelectModel` value instructs Windows to invoke the command separately for each selected item, ensuring that every highlighted file is forwarded to PushToFolders even though the command line uses `%1`.

> **Note:** If you previously imported an older script that used the `HKEY_CLASSES_ROOT` hive or `%*`, remove it with the uninstall snippet below before adding the new entry so that Windows picks up the corrected command line.

To remove the menu entry later, create another file named `Remove_PushToFolders_ContextMenu.reg` with the following content and run it:

```reg
Windows Registry Editor Version 5.00

[-HKEY_CURRENT_USER\Software\Classes\*\Shell\PushToFolders]
```

## Troubleshooting

* **Nothing happens:** Check the console output or run `PushToFolders --show-log` to inspect the log file. The log will explain whether the selected items were skipped.
* **Folder already exists:** If the destination folder already contains a file with the same name, the program leaves the original file untouched and reports the error. Rename or remove the conflicting file, then run the utility again.
* **Permission errors:** Ensure that you have permission to create folders and move files in the target location. If necessary, move the files into a folder where you have write access and run the tool again.

## License

This project is released into the public domain. Use it without restriction.
