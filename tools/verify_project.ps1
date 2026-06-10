param(
    [string]$BuildDir = "build\Desktop_Qt_6_11_1_MinGW_64_bit-Debug"
)

$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$resolvedBuildDir = Resolve-Path (Join-Path $repoRoot $BuildDir)
$unitsPath = Join-Path $repoRoot "data\units.json"
$skillsPath = Join-Path $repoRoot "data\skills.json"
$equipmentPath = Join-Path $repoRoot "data\equipment.json"
$traitsPath = Join-Path $repoRoot "data\traits.json"
$eventsPath = Join-Path $repoRoot "data\events.json"
$enemyWavesPath = Join-Path $repoRoot "data\enemy_waves.json"

$unitsConfig = Get-Content -LiteralPath $unitsPath -Raw -Encoding UTF8 | ConvertFrom-Json
$skillsConfig = Get-Content -LiteralPath $skillsPath -Raw -Encoding UTF8 | ConvertFrom-Json
$equipmentConfig = Get-Content -LiteralPath $equipmentPath -Raw -Encoding UTF8 | ConvertFrom-Json
$traitsConfig = Get-Content -LiteralPath $traitsPath -Raw -Encoding UTF8 | ConvertFrom-Json
$eventsConfig = Get-Content -LiteralPath $eventsPath -Raw -Encoding UTF8 | ConvertFrom-Json
$enemyWavesConfig = Get-Content -LiteralPath $enemyWavesPath -Raw -Encoding UTF8 | ConvertFrom-Json

$skillTypes = @{}
foreach ($skill in $skillsConfig.skills) {
    $skillTypes[$skill.type] = $true
}

$unitNames = @{}
foreach ($unit in $unitsConfig.units) {
    if (-not $unit.name) {
        throw "units.json contains a unit without name."
    }
    $unitNames[$unit.name] = $true
    if (-not $skillTypes.ContainsKey($unit.skillType)) {
        throw "Unit '$($unit.name)' references missing skill '$($unit.skillType)'."
    }
}

foreach ($equipment in $equipmentConfig.equipment) {
    if (-not $equipment.type -or -not $equipment.name) {
        throw "equipment.json contains invalid equipment."
    }
}

foreach ($trait in $traitsConfig.traits) {
    if (-not $trait.name -or -not $trait.thresholds) {
        throw "traits.json contains invalid trait."
    }
}

foreach ($event in $eventsConfig.events) {
    if (-not $event.key -or $event.every -le 0) {
        throw "events.json contains invalid event."
    }
}

foreach ($enemy in $enemyWavesConfig.enemies) {
    if (-not $unitNames.ContainsKey($enemy.name)) {
        throw "Enemy wave references missing unit '$($enemy.name)'."
    }
}

Push-Location $repoRoot
try {
    cmake --build $resolvedBuildDir

    $cachePath = Join-Path $resolvedBuildDir "CMakeCache.txt"
    $cacheLines = Get-Content -LiteralPath $cachePath -Encoding UTF8
    $qtPrefix = ($cacheLines | Where-Object { $_ -like "CMAKE_PREFIX_PATH:PATH=*" } | Select-Object -First 1) -replace "^CMAKE_PREFIX_PATH:PATH=", ""
    $compiler = ($cacheLines | Where-Object { $_ -like "CMAKE_CXX_COMPILER:STRING=*" } | Select-Object -First 1) -replace "^CMAKE_CXX_COMPILER:STRING=", ""
    $qtBin = Join-Path $qtPrefix "bin"
    $compilerBin = Split-Path $compiler -Parent
    $env:PATH = "$qtBin;$compilerBin;$env:PATH"

    $exe = Join-Path $resolvedBuildDir "Synera_Starter.exe"
    if (-not (Test-Path -LiteralPath $exe)) {
        throw "Executable not found: $exe"
    }

    $process = Start-Process -FilePath $exe -WorkingDirectory $resolvedBuildDir -PassThru
    Start-Sleep -Seconds 3
    $alive = -not $process.HasExited
    if ($alive) {
        Stop-Process -Id $process.Id
    } elseif ($process.ExitCode -ne 0) {
        throw "Application exited early with code $($process.ExitCode)."
    }

    Write-Host "Verification passed."
} finally {
    Pop-Location
}
