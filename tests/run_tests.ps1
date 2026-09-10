# cella_sql 回归测试脚本
# 用法: powershell -ExecutionPolicy Bypass -File tests\run_tests.ps1
# 正向: 退出码 0，且 -p 输出与 tests/expected/<name>_plan.txt golden 一致
# 负向: 退出码 1，且输出包含期望错误码（SQL 文件首行注释 -- expect: CODE）

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$candidates = @(
    (Join-Path $root "build\Debug\cella_sql.exe"),
    (Join-Path $root "build\Release\cella_sql.exe"),
    (Join-Path $root "build\MinSizeRel\cella_sql.exe"),
    (Join-Path $root "build\RelWithDebInfo\cella_sql.exe"),
    (Join-Path $root "build\cella_sql.exe")
)
$exe = $null
foreach ($c in $candidates) {
    if (Test-Path $c) { $exe = $c; break }
}
if (-not $exe) {
    Write-Host "[失败] 未找到 cella_sql.exe，请先执行 cmake 构建。"
    exit 1
}
Write-Host "使用: $exe"

$pass = 0
$fail = 0

$utf8 = New-Object System.Text.UTF8Encoding($false)

# 通过 cmd 重定向捕获原始输出字节，返回 @{ out = ...; code = ... }
function CaptureRaw([string]$argsStr, [string]$sqlPath) {
    $tmp = [IO.Path]::GetTempFileName()
    $cmdLine = "`"$exe`" $argsStr `"$sqlPath`" > `"$tmp`" 2>&1"
    cmd /c $cmdLine | Out-Null
    $code = $LASTEXITCODE
    $s = [IO.File]::ReadAllText($tmp, $utf8)
    [IO.File]::Delete($tmp)
    return @{ out = $s; code = $code }
}

function Normalize([string]$s) {
    return ($s -replace "`r`n", "`n").TrimEnd()
}

# ---------- 正向用例 ----------
Get-ChildItem (Join-Path $PSScriptRoot "sql\ok_*.sql") | Sort-Object Name | ForEach-Object {
    $name = $_.BaseName
    $sql = $_.FullName
    $expected = Join-Path $PSScriptRoot ("expected\" + $name + "_plan.txt")
    if (-not (Test-Path $expected)) {
        Write-Host "[失败] $name : 缺少 golden 文件 expected\${name}_plan.txt"
        $fail++
        return
    }
    # 全阶段退出码检查
    $r1 = CaptureRaw "-a -s -p" $sql
    if ($r1.code -ne 0) {
        Write-Host "[失败] $name : -a -s -p 退出码 $($r1.code) (期望 0)"
        $fail++
        return
    }
    # -p 输出与 golden 比较
    $r2 = CaptureRaw "-p" $sql
    $exp = Normalize ([IO.File]::ReadAllText($expected, $utf8))
    if ((Normalize $r2.out) -ne $exp) {
        Write-Host "[失败] $name : -p 输出与 golden 不一致"
        $fail++
        return
    }
    # ok_opt_* 额外比对 -o 输出
    if ($name -like "ok_opt_*") {
        $expectedOpt = Join-Path $PSScriptRoot ("expected\" + $name + "_opt.txt")
        if (-not (Test-Path $expectedOpt)) {
            Write-Host "[失败] $name : 缺少 golden 文件 expected\${name}_opt.txt"
            $fail++
            return
        }
        $r3 = CaptureRaw "-o" $sql
        $expOpt = Normalize ([IO.File]::ReadAllText($expectedOpt, $utf8))
        if ((Normalize $r3.out) -ne $expOpt) {
            Write-Host "[失败] $name : -o 输出与 golden 不一致"
            $fail++
            return
        }
    }
    Write-Host "[通过] $name"
    $pass++
}

# ---------- 负向用例 ----------
Get-ChildItem (Join-Path $PSScriptRoot "sql\err_*.sql") | Sort-Object Name | ForEach-Object {
    $name = $_.BaseName
    $sql = $_.FullName
    $expectedCode = ""
    $firstLine = Get-Content $sql -TotalCount 1
    if ($firstLine -match 'expect\s*:\s*([A-Z]+-\d+)') { $expectedCode = $matches[1] }
    $r = CaptureRaw "--all" $sql
    if ($r.code -ne 1) {
        Write-Host "[失败] $name : --all 退出码 $($r.code) (期望 1)"
        $fail++
        return
    }
    if ($expectedCode -and ($r.out -notmatch [regex]::Escape($expectedCode))) {
        Write-Host "[失败] $name : 输出未包含期望错误码 $expectedCode"
        $fail++
        return
    }
    Write-Host "[通过] $name"
    $pass++
}

Write-Host ""
Write-Host "========== 汇总: 通过 $pass / 失败 $fail =========="
if ($fail -gt 0) { exit 1 } else { exit 0 }
