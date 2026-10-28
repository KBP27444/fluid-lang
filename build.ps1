param(
    [switch]$Debug,
    [switch]$Sanitize
)

$ErrorActionPreference = "Stop"
$Compiler = "C:\msys64\ucrt64\bin\g++.exe"

if (-not (Test-Path $Compiler)) {
    Write-Error "Compiler not found: $Compiler"
    exit 1
}

$SourceDir = Join-Path $PSScriptRoot "src"
$Output = Join-Path $PSScriptRoot "fluid.exe"
$Sources = Get-ChildItem -Path (Join-Path $SourceDir "*.cpp") | ForEach-Object { $_.FullName }

if ($Sources.Count -eq 0) {
    Write-Error "No C++ source files found in $SourceDir"
    exit 1
}

$Arguments = @("-std=c++17", "-Wall", "-Wextra", "-pedantic")
if ($Debug) {
    $Arguments += @("-g", "-O0")
}
if ($Sanitize) {
    # 先用最小程序探测 ASan 是否可用；不可用则回退 -Debug（不阻塞验收）
    $ProbeDir = $PSScriptRoot
    if ([string]::IsNullOrEmpty($ProbeDir)) {
        $ProbeDir = Split-Path -Parent $MyInvocation.MyCommand.Path
    }
    $ProbeSource = [System.IO.Path]::Combine([string]$ProbeDir, "lang_asan_probe.cpp")
    $ProbeExe = [System.IO.Path]::Combine([string]$ProbeDir, "lang_asan_probe.exe")
    Set-Content -Path ([string]$ProbeSource) -Value "int main(){return 0;}" -Encoding ASCII

    $ProbeOk = $false
    try {
        & $Compiler "-std=c++17" "-g" "-O1" "-fsanitize=address" $ProbeSource "-o" $ProbeExe 2>$null
        if ($LASTEXITCODE -eq 0 -and (Test-Path $ProbeExe)) {
            & $ProbeExe 2>$null
            if ($LASTEXITCODE -eq 0) {
                $ProbeOk = $true
            }
        }
    } catch {
        $ProbeOk = $false
    }

    Remove-Item -Path $ProbeSource -ErrorAction SilentlyContinue
    Remove-Item -Path $ProbeExe -ErrorAction SilentlyContinue

    if ($ProbeOk) {
        Write-Host "ASan 可用，使用 -fsanitize=address 构建。"
        $Arguments += @("-g", "-O1", "-fsanitize=address")
    } else {
        Write-Host "ASan 不可用，跳过（不阻塞验收），回退 -Debug。"
        $Arguments += @("-g", "-O0")
    }
}
$Arguments += $Sources
$Arguments += @("-o", $Output)

& $Compiler @Arguments
exit $LASTEXITCODE
