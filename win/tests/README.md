# Windows unit tests

## Prerequisites

- Windows with PowerShell.
- Visual Studio or Visual Studio Build Tools with the **Desktop development with
  C++** workload and a Windows SDK.
- MSVC build tools for the target architecture (`arm64`, `x64`, or `x86`).
  For ARM64, install the ARM64 C++ build tools in Visual Studio Installer.
- The machine must be able to execute binaries for the selected architecture.

No third-party test framework is required. The runner locates Visual Studio using
`vswhere.exe` and configures the compiler automatically; a Developer PowerShell
session is not required.

## Run the tests

From the repository root:

```powershell
.\win\tests\run-tests.ps1
```

Or, from this directory:

```powershell
.\run-tests.ps1
```

By default, the runner targets the current PowerShell process architecture.
To select a target explicitly, run one of these commands from the repository root:

```powershell
.\win\tests\run-tests.ps1 -Architecture arm64
.\win\tests\run-tests.ps1 -Architecture x64
.\win\tests\run-tests.ps1 -Architecture x86
```

Each invocation builds and runs the entire suite for one architecture. Build
files go into a unique temporary directory and are removed after the run. No
changes to the Visual Studio solution or project settings are necessary.

## Expected results

Each test prints `PASS` or `FAIL`, followed by a summary:

```text
PASS: reads foreground thread layout
...
14 passed, 0 failed
```

The runner throws an error if compilation or a test fails. When invoked with
`powershell -File` or `pwsh -File`, this produces a nonzero process exit code,
which can be used in automation:

```powershell
pwsh -NoProfile -File .\win\tests\run-tests.ps1 -Architecture arm64
```

## Coverage and limitations

[im-select-tests.cpp](im-select-tests.cpp) compiles the existing production
implementation with Windows API calls and console output replaced by test fakes.
The tests cover:

- Reading the foreground window's thread and keyboard layout.
- Extracting the low 16-bit language ID, including pointer-width handling.
- Missing foreground windows and zero keyboard layouts.
- Posting `WM_INPUTLANGCHANGEREQUEST` with the expected parameters.
- CLI output, locale parsing, additional arguments, and exit status.

Characterization tests intentionally preserve current behavior, including
permissive `atoi` parsing, posting with no foreground window, and ignoring
`PostMessage` failures. These are baseline observations, not recommended policies.

Tests do not change your keyboard layout or verify real OS input-method
switching. They also do not validate the production solution's build settings.

## Troubleshooting

- **Visual Studio or C++ tools not found:** install the workload and target tools
  listed above using Visual Studio Installer.
- **Compiler setup or SDK errors:** confirm that the selected architecture's
  compiler and a Windows SDK are installed.
- **PowerShell blocks the script:** follow your organization's script-execution
  policy; do not change machine-wide policy just to run these tests.
- **Pointer-truncation warnings on ARM64/x64:** the current production source
  emits C4311 and C4302. These warnings do not prevent the tests from running.
- **Cleanup retry warning:** Windows may briefly lock a test executable after it
  exits. The runner retries cleanup up to five times and reports an error if
  cleanup still fails.
