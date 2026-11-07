param(
    [ValidateSet('arm64', 'x64', 'x86')]
    [string]$Architecture = $(switch ($env:PROCESSOR_ARCHITECTURE) {
        'ARM64' { 'arm64' }
        'AMD64' { 'x64' }
        default { 'x86' }
    })
)

$ErrorActionPreference = 'Stop'
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path -LiteralPath $vswhere)) {
    throw 'Visual Studio Installer (vswhere.exe) is required to locate the C++ build tools.'
}

$setup = & $vswhere -latest -products '*' -requiresAny -requires `
    Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
    Microsoft.VisualStudio.Component.VC.Tools.ARM64 `
    -find 'VC\Auxiliary\Build\vcvarsall.bat'
if ($LASTEXITCODE -ne 0 -or -not $setup) {
    throw 'Visual Studio C++ build tools were not found. Install the Desktop development with C++ workload.'
}

$hostArchitecture = switch ($env:PROCESSOR_ARCHITECTURE) {
    'ARM64' { 'arm64' }
    'AMD64' { 'amd64' }
    default { 'x86' }
}
$targetArchitecture = if ($Architecture -eq 'x64') { 'amd64' } else { $Architecture }
$compilerArchitecture = if ($hostArchitecture -eq $targetArchitecture) {
    $hostArchitecture
} else {
    "${hostArchitecture}_${targetArchitecture}"
}
$source = Join-Path $PSScriptRoot 'im-select-tests.cpp'
$solution = Join-Path $PSScriptRoot '..\im-select\im-select.sln'
$msbuild = Join-Path (Split-Path -Parent $setup) '..\..\..\MSBuild\Current\Bin\MSBuild.exe'
$buildDirectory = Join-Path ([System.IO.Path]::GetTempPath()) (
    'im-select-tests-' + [guid]::NewGuid().ToString('N')
)
$solutionBuildDirectory = Join-Path $PSScriptRoot (
    '.im-select-build-' + [guid]::NewGuid().ToString('N')
)
New-Item -ItemType Directory -Path $buildDirectory | Out-Null

try {
    if ($Architecture -eq 'arm64') {
        Write-Warning 'Skipping the production solution build: it does not define an ARM64 configuration.'
    } else {
        if (-not (Test-Path -LiteralPath $msbuild)) {
            throw 'MSBuild was not found in the selected Visual Studio installation.'
        }
        Write-Output "Building the production solution (Release|$Architecture)"
        New-Item -ItemType Directory -Path $solutionBuildDirectory | Out-Null
        $outputDirectory = (Join-Path $solutionBuildDirectory 'output') + '\'
        $intermediateDirectory = (Join-Path $solutionBuildDirectory 'intermediate') + '\'
        & $msbuild $solution /nologo /t:Build '/p:Configuration=Release' "/p:Platform=$Architecture" `
            "/p:OutDir=$outputDirectory" "/p:IntDir=$intermediateDirectory"
        if ($LASTEXITCODE -ne 0) {
            throw "Production solution build failed (exit code $LASTEXITCODE)."
        }
    }

    Write-Output "Building and running Windows unit tests ($Architecture)"
    $command = 'call "{0}" {1} >nul && cd /d "{2}" && cl /nologo /EHsc /std:c++14 /W4 /MT /Od /DUNICODE /D_UNICODE /Fe:im-select-tests.exe "{3}" && im-select-tests.exe' -f (
        $setup, $compilerArchitecture, $buildDirectory, $source
    )
    & $env:ComSpec /d /c $command
    if ($LASTEXITCODE -ne 0) {
        throw "Windows unit test build or execution failed (exit code $LASTEXITCODE)."
    }
} finally {
    foreach ($directory in @($buildDirectory, $solutionBuildDirectory)) {
        if (Test-Path -LiteralPath $directory) {
            for ($attempt = 1; $attempt -le 5; ++$attempt) {
                try {
                    Remove-Item -LiteralPath $directory -Recurse -Force
                    break
                } catch [System.IO.IOException], [System.UnauthorizedAccessException] {
                    if ($attempt -eq 5) {
                        throw
                    }
                    Write-Warning "Build files are still in use; retrying cleanup ($attempt/5)."
                    Start-Sleep -Milliseconds (200 * $attempt)
                }
            }
        }
    }
}
