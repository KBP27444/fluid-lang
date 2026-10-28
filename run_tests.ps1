param(
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$TestArguments
)

$ErrorActionPreference = "Stop"
$TestRunner = Join-Path $PSScriptRoot "tests\run_tests.py"

python $TestRunner @TestArguments
exit $LASTEXITCODE
