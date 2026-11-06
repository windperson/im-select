# Copilot Instructions for im-select

## Build, Test, and Lint
- **Windows Unit Tests**: Run using `.\win\tests\run-tests.ps1`. This script automatically detects architecture, locates C++ build tools, and compiles `win\tests\im-select-tests.cpp`.
- **Windows Build**: The main project is managed via the Visual Studio solution at `win\im-select\im-select.sln`.
- **macOS Build**: Managed via the Xcode project at `macOS\im-select\im-select.xcodeproj`.
- **win-mspy Build**: Uses CMake as defined in `win-mspy\CMakeLists.txt`.
- **Linting**: No automated linting scripts were identified; follow standard language-specific best practices (C++, Objective-C, Shell).

## High-level Architecture
- **Purpose**: A multi-platform CLI tool (`im-select`) providing input method (IM) switching. It is primarily a backend for `VSCodeVim`'s `autoSwitchIM` function.
- **Platform Separation**: The repository is organized by platform to handle specific OS keyboard APIs:
    - `win/`: C++ implementation for Windows.
    - `macOS/`: Objective-C/C implementation for macOS.
    - `win-mspy/`: Specialized Windows implementation.
- **Workflow**: The tool is designed to be called from a shell (specifically `git-bash` on Windows) to retrieve or set the current keyboard layout.

## Key Conventions
- **Windows Shell**: Always assume `git-bash` is the preferred environment for Windows users because standard `cmd.exe` and `powershell.exe` have limitations with the keyboard APIs used.
- **Platform Specifics**: Changes in the `win/` directory should focus on Win32 API patterns, while `macOS/` focuses on Cocoa/CoreFoundation.
- **Test Mocking**: Windows unit tests use mocked Windows APIs to allow testing without changing actual system settings.
