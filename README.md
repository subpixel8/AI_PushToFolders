# PushToFolders

PushToFolders is a tiny Windows command line utility that sorts image files into folders that share their names. Each image is moved into a sibling folder whose name matches the file name (without the extension).

The executable supports two usage scenarios:

1. **Command line** — supply a single folder path and every supported image inside it is moved into a like-named folder.
2. **Windows File Explorer context menu** — select one or more supported image files, invoke the tool, and each selected file is moved into its own folder.

All errors are written to a persistent log file so that you always have an audit trail of what happened.

## Supported image formats

* `.jpg`
* `.jpeg`
* `.png`
* `.bmp`
* `.webp`

The comparison is case-insensitive.

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
PushToFolders "C:\Users\you\Pictures\holiday.jpg" "D:\More Photos\portrait.png"
PushToFolders --show-log
PushToFolders --clear-log
```

> **Important:** The program fully supports paths that contain characters such as ampersands (`&`), emoji, or characters from other languages. The Windows command interpreter treats `&` as a command separator, so if you type a command manually and the path contains `&`, escape it as `^&` (for example, `"C:\Games^&Art\cover.jpg"`). No extra steps are required when launching the tool from File Explorer.

* When you pass exactly one argument and it is a folder, the program scans it for supported image files.
* When you pass one or more file paths, every supported image is moved into a folder named after the file.
* `--show-log` prints the error log, and `--clear-log` erases the log file so that the next run starts fresh.

Successful operations are echoed to the console, while errors are written both to the console (when available) and to the log file.

> **Tip:** When the tool is started from File Explorer it does not display a console window. Run `PushToFolders --show-log` later to review the log or use the command line directly if you want to watch progress in real time.

### Log file location

The log file is stored inside your `%LOCALAPPDATA%\PushToFolders` folder. If that folder cannot be created, the program falls back to the system temporary directory.

## Adding the Windows File Explorer context menu entry

The context menu is configured with a small registry script. The script adds a **"Push images into folders"** option when you right-click supported files.

1. Open *Notepad*.
2. Paste the snippet below and adjust the path so that it points to the actual location of `PushToFolders.exe`.

   ```reg
   Windows Registry Editor Version 5.00

   [HKEY_CLASSES_ROOT\SystemFileAssociations\.jpg\Shell\PushToFolders]
   @="Push images into folders"
   "Icon"="\"C:\\Program Files\\PushToFolders\\PushToFolders.exe\""

   [HKEY_CLASSES_ROOT\SystemFileAssociations\.jpg\Shell\PushToFolders\Command]
   @="\"C:\\Program Files\\PushToFolders\\PushToFolders.exe\" \"%*\""

   [HKEY_CLASSES_ROOT\SystemFileAssociations\.jpeg\Shell\PushToFolders]
   @="Push images into folders"
   "Icon"="\"C:\\Program Files\\PushToFolders\\PushToFolders.exe\""

   [HKEY_CLASSES_ROOT\SystemFileAssociations\.jpeg\Shell\PushToFolders\Command]
   @="\"C:\\Program Files\\PushToFolders\\PushToFolders.exe\" \"%*\""

   [HKEY_CLASSES_ROOT\SystemFileAssociations\.png\Shell\PushToFolders]
   @="Push images into folders"
   "Icon"="\"C:\\Program Files\\PushToFolders\\PushToFolders.exe\""

   [HKEY_CLASSES_ROOT\SystemFileAssociations\.png\Shell\PushToFolders\Command]
   @="\"C:\\Program Files\\PushToFolders\\PushToFolders.exe\" \"%*\""

   [HKEY_CLASSES_ROOT\SystemFileAssociations\.bmp\Shell\PushToFolders]
   @="Push images into folders"
   "Icon"="\"C:\\Program Files\\PushToFolders\\PushToFolders.exe\""

   [HKEY_CLASSES_ROOT\SystemFileAssociations\.bmp\Shell\PushToFolders\Command]
   @="\"C:\\Program Files\\PushToFolders\\PushToFolders.exe\" \"%*\""

   [HKEY_CLASSES_ROOT\SystemFileAssociations\.webp\Shell\PushToFolders]
   @="Push images into folders"
   "Icon"="\"C:\\Program Files\\PushToFolders\\PushToFolders.exe\""

   [HKEY_CLASSES_ROOT\SystemFileAssociations\.webp\Shell\PushToFolders\Command]
   @="\"C:\\Program Files\\PushToFolders\\PushToFolders.exe\" \"%*\""
   ```

3. Save the file as `PushToFolders_ContextMenu.reg`.
4. Double-click the saved file and confirm the prompts to add it to the registry.

The command above registers the same handler for every supported file extension. When you select multiple files and choose **Push images into folders**, Windows passes all selected paths to the executable in one invocation.

To remove the menu entry later, create another file named `Remove_PushToFolders_ContextMenu.reg` with the following content and run it:

```reg
Windows Registry Editor Version 5.00

[-HKEY_CLASSES_ROOT\SystemFileAssociations\.jpg\Shell\PushToFolders]
[-HKEY_CLASSES_ROOT\SystemFileAssociations\.jpeg\Shell\PushToFolders]
[-HKEY_CLASSES_ROOT\SystemFileAssociations\.png\Shell\PushToFolders]
[-HKEY_CLASSES_ROOT\SystemFileAssociations\.bmp\Shell\PushToFolders]
[-HKEY_CLASSES_ROOT\SystemFileAssociations\.webp\Shell\PushToFolders]
```

## Troubleshooting

* **Nothing happens:** The program skips files that are not one of the supported image formats. Check the console output or run `PushToFolders --show-log` to inspect the log file.
* **Folder already exists:** If the destination folder already contains a file with the same name, the program leaves the original file untouched and reports the error. Rename or remove the conflicting file, then run the utility again.
* **Permission errors:** Ensure that you have permission to create folders and move files in the target location. If necessary, move the files into a folder where you have write access and run the tool again.

## License

This project is released into the public domain. Use it without restriction.
