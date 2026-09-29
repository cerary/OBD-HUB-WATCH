# Run from any directory. Clones the M5 components next to this repository.
# Versions match the physically verified StopWatch display bring-up.
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$parent = Split-Path $repo -Parent
$deps = @(
    @{ Name = 'M5GFX'; Version = '0.2.19'; Commit = '53a7184'; Url = 'https://github.com/m5stack/M5GFX.git' },
    @{ Name = 'M5PM1'; Version = '1.0.6'; Commit = '8f1f1a6'; Url = 'https://github.com/m5stack/M5PM1.git' },
    @{ Name = 'M5IOE1'; Version = '1.0.8'; Commit = '37db048'; Url = 'https://github.com/m5stack/M5IOE1.git' }
)

foreach ($dep in $deps) {
    $path = Join-Path $parent $dep.Name
    if (-not (Test-Path -LiteralPath $path)) {
        git clone --depth 1 --branch $dep.Version $dep.Url $path
        if ($LASTEXITCODE -ne 0) { throw "Could not clone $($dep.Name)" }
    }
    $actual_line = git -c "safe.directory=$path" -C $path rev-parse --short=7 HEAD
    if ($LASTEXITCODE -ne 0 -or -not $actual_line) {
        throw "Could not inspect $($dep.Name) at $path"
    }
    $actual = $actual_line.Trim()
    if ($actual -ne $dep.Commit) {
        throw "$($dep.Name) is at $actual; expected $($dep.Commit). Inspect it before building."
    }
}

# The StopWatch G page uses Bosch's official BMI270 sensor API directly on the
# same new-style ESP-IDF I2C bus as the display/touch bridge.
$bmiPath = Join-Path $parent 'BMI270_SensorAPI'
$bmiCommit = '41129fcfe39c583ee5462d79195741945d51c1fe'
if (-not (Test-Path -LiteralPath $bmiPath)) {
    git clone --depth 1 https://github.com/boschsensortec/BMI270_SensorAPI.git $bmiPath
    if ($LASTEXITCODE -ne 0) { throw 'Could not clone BMI270_SensorAPI' }
    # The remote default branch can move. Fetch and detach the reviewed commit
    # so a fresh project checkout builds with the same sensor API revision.
    git -C $bmiPath fetch --depth 1 origin $bmiCommit
    if ($LASTEXITCODE -ne 0) { throw "Could not fetch BMI270_SensorAPI revision $bmiCommit" }
    git -C $bmiPath checkout --detach $bmiCommit
    if ($LASTEXITCODE -ne 0) { throw "Could not check out BMI270_SensorAPI revision $bmiCommit" }
}
$bmiActual = git -c "safe.directory=$bmiPath" -C $bmiPath rev-parse HEAD
if ($LASTEXITCODE -ne 0 -or $bmiActual.Trim() -ne $bmiCommit) {
    throw "BMI270_SensorAPI revision differs from $bmiCommit. Inspect it before building."
}

# These two driver headers select an I2C ABI with __has_include(<M5GFX.h>).
# Every translation unit must see the same M5GFX include path, or global C++
# objects have different layouts and overwrite the neighboring display object.
foreach ($name in @('M5PM1', 'M5IOE1')) {
    $file = Join-Path $parent "$name\CMakeLists.txt"
    $source = Get-Content -LiteralPath $file -Raw
    if ($source -notmatch '"M5GFX"') {
        $source = $source.Replace('"espressif__i2c_bus"', '"espressif__i2c_bus"' + "`n        `"M5GFX`"")
        if ($source -notmatch '"M5GFX"') { throw "Could not patch $file" }
        [System.IO.File]::WriteAllText($file, $source)
    }
}

# M5GFX 0.2.19 dereferences a failed DMA allocation in the AMOLED frame
# buffer transfer. BLE can exhaust/fragment internal DMA heap after boot.
$amoledFile = Join-Path $parent 'M5GFX\src\lgfx\v1\panel\Panel_AMOLED.cpp'
$amoled = Get-Content -LiteralPath $amoledFile -Raw
if ($amoled -notmatch 'Send the PSRAM row through the SPI') {
    $old = '                auto lb = buf[i & 1];//s->getDMABuffer(wb);' + "`n" +
        '                memcpy(lb,  &_frame_buffer[fbpos], wb);' + "`n" +
        '                fbpos += stride; // next line' + "`n" +
        '                bus->writeBytes(lb, wb, false, true);'
    $new = '                auto lb = buf[i & 1];//s->getDMABuffer(wb);' + "`n" +
        '                if (lb)' + "`n" +
        '                {' + "`n" +
        '                    memcpy(lb, &_frame_buffer[fbpos], wb);' + "`n" +
        '                }' + "`n" +
        '                fbpos += stride; // next line' + "`n" +
        '                // The SPI flip buffers can fail to resize when BLE fragments' + "`n" +
        '                // internal DMA memory. Send the PSRAM row through the SPI' + "`n" +
        '                // register path instead of dereferencing a null buffer.' + "`n" +
        '                bus->writeBytes(lb ? lb : &_frame_buffer[fbpos - stride], wb, false, lb != nullptr);'
    $normalized = $amoled.Replace("`r`n", "`n")
    if (-not $normalized.Contains($old)) { throw "Unexpected AMOLED transfer code in $amoledFile" }
    [System.IO.File]::WriteAllText($amoledFile, $normalized.Replace($old, $new))
}
Write-Host 'StopWatch dependencies ready.'
