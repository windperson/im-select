# Windows unit tests

## Prerequisites

- Windows with PowerShell.
- Visual Studio or Visual Studio Build Tools with the **Desktop development with
  C++** workload and a Windows SDK.
- MSVC build tools for the target architecture (`arm64`, `x64`, or `x86`).
  For ARM64, install the ARM64 C++ build tools in Visual Studio Installer.
- MSBuild in the selected Visual Studio installation for production solution builds.
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

Each invocation builds and runs the entire suite for one architecture. For x86
and x64, it first builds the matching Release configuration of the production
solution. The solution currently has no ARM64 configuration, so an ARM64 run
warns that the production build was skipped and still runs the unit tests.
Test files go into a unique temporary directory; production build files go into
a unique directory under `win/tests` to avoid MSBuild's temporary-directory
warning. Both directories are removed after the run. No changes to the Visual
Studio solution or project settings are necessary.

## Expected results

Each test prints `PASS` or `FAIL`, followed by a summary:

```text
PASS: reads foreground thread layout
...
18 passed, 0 failed
```

The runner throws an error if the production build, test compilation, or a test
fails. When invoked with
`powershell -File` or `pwsh -File`, this produces a nonzero process exit code,
which can be used in automation:

```powershell
pwsh -NoProfile -File .\win\tests\run-tests.ps1 -Architecture arm64
```

## Optional integration test

Run the integration test from an active, unlocked Windows desktop using x86 or
x64. It builds the production executable and runs the unit tests first:

```powershell
.\win\tests\run-tests.ps1 -Architecture x64 -Integration
```

The test briefly opens a window and must make it the foreground window. Do not
switch windows while it runs: the production executable targets whichever
window is foreground, so a focus change could direct its request elsewhere.
The test checks focus before and after each invocation and fails if it cannot
reliably use its own window. It attempts to restore the previous foreground
window afterward.

The test checks the actual CLI output and intercepts the language-change
request in its own window without applying it. It verifies message delivery,
not whether Windows or an IME would accept the request or switch layouts. It is
not part of the default test run or CI. ARM64 integration testing is unavailable
until the production solution has an ARM64 configuration.

## Continuous integration

The [Windows tests workflow](../../.github/workflows/windows-tests.yml) runs on
pushes, pull requests, and manual dispatch. It uses Visual Studio 2026 hosted
images to run x86 and x64 tests on Windows Server 2025 and ARM64 tests on a
native Windows 11 ARM64 runner. The x86 and x64 jobs also build the matching
Release production solution; the ARM64 job only builds and runs the tests.
Runner availability and usage limits depend on the repository and GitHub plan.

## Coverage and limitations

[im-select-tests.cpp](im-select-tests.cpp) compiles the existing production
implementation with Windows API calls and console output replaced by test fakes.
The tests cover:

- Reading the foreground window's thread and keyboard layout.
- Extracting the low 16-bit language ID, including pointer-width handling.
- Missing foreground windows and zero keyboard layouts.
- Posting `WM_INPUTLANGCHANGEREQUEST` with the expected parameters.
- CLI output, validated decimal locale parsing, additional arguments, error
  diagnostics, and exit status.

The tests verify nonzero exit status and stderr diagnostics for malformed or
out-of-range input, a missing foreground window, query failures, and a failed
`PostMessage`. Additional arguments after the first locale are still ignored.
Successful posting does not prove that the window applied the request.

The mocked tests do not change your keyboard layout or verify real OS input-method
switching. The production solution build validates the Release configuration for
x86 and x64; it does not validate Debug or an ARM64 production configuration.

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
