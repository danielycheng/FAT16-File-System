# FAT16 File System

## Problem
This program implements a command line tool that can inspect and manipulate FAT16 file system data. The program acts like a very primitive shell/file-manager that bridges two file system namespaces: (1) the unified file system exposed by the shell that launched your program, and (2) the FAT16 file system that your file-manager will load.

Commands are read from the prompt, one line at a time. Your program will implement the following five commands:
- **cd**: [directory] Changes the current working directory of your loaded file system to the relative path specified by [directory]
- **ls**: [directory] Displays the files within <directory>, or the current working directory if no argument is given
- **cpin**: [src] [dst] Copies the contents of a file specified by the path [src] from the “real” unified file system (exposed by the shell that launched your program) into the file system loaded by your program, linking it there as the path specified by [dst]
- **cpout**: [src] [dst] Copies the contents of a file specified by the path [src] from the file system loaded by your program into the “real” unified file system (exposed by the shell that launched your program), linking it there as the path specified by [dst]
- **exit**: Exits your program.

