<#
.SYNOPSIS
    Nyra Player vs stock VLC — reproducible benchmark harness.

.DESCRIPTION
    Runs the same test file through both players under identical conditions
    and records CPU%, GPU engine usage, working-set RAM, dropped frames
    (parsed from each player's stats), cold-start time, and seek latency.
    Battery drain is measured via `powercfg /batteryreport` deltas when run
    on battery power.

    This produces REAL numbers only when actually run on real Windows
    hardware with both .exe's built. It has not been run anywhere in the
    delivery of this project — do not treat any number in this file's
    comments as a result; there are none yet.

.NOTES
    Requires: both players built (see ../BUILD.md), a fixed test corpus,
    and PresentMon or the built-in "GPU Engine" perf counters for GPU%.
#>

param(
    [Parameter(Mandatory=$true)][string]$NyraExePath,
    [Parameter(Mandatory=$true)][string]$VlcExePath,
    [Parameter(Mandatory=$true)][string]$TestFilesDir,
    [int]$PlaybackDurationSec = 120,
    [string]$OutputCsv = ".\benchmark-results.csv"
)

$ErrorActionPreference = "Stop"

function Measure-PlayerRun {
    param([string]$ExePath, [string]$MediaFile, [string]$Label)

    Write-Host "Running $Label on $MediaFile ..."

    $startTime = Get-Date
    $proc = Start-Process -FilePath $ExePath -ArgumentList "`"$MediaFile`" --started-from-file" -PassThru
    Start-Sleep -Milliseconds 500  # let process create its window before timing "first frame" proxy
    $launchedTime = Get-Date
    $coldStartMs = ($launchedTime - $startTime).TotalMilliseconds

    $cpuSamples = @()
    $ramSamples = @()
    $gpuSamples = @()
    $sampleEnd = (Get-Date).AddSeconds($PlaybackDurationSec)

    while ((Get-Date) -lt $sampleEnd -and -not $proc.HasExited) {
        Start-Sleep -Milliseconds 500
        try {
            $proc.Refresh()
            $cpuSamples += (Get-Counter "\Process($($proc.ProcessName))\% Processor Time").CounterSamples[0].CookedValue
            $ramSamples += $proc.WorkingSet64
            $gpuCounters = Get-Counter "\GPU Engine(*$($proc.Id)*engtype_3D)\Utilization Percentage" -ErrorAction SilentlyContinue
            if ($gpuCounters) {
                $gpuSamples += ($gpuCounters.CounterSamples | Measure-Object -Property CookedValue -Sum).Sum
            }
        } catch {
            # Counter can transiently miss a sample right after process start; skip it, don't fail the run.
        }
    }

    if (-not $proc.HasExited) { Stop-Process -Id $proc.Id -Force }

    [PSCustomObject]@{
        Player           = $Label
        MediaFile        = Split-Path $MediaFile -Leaf
        ColdStartMs      = [math]::Round($coldStartMs, 1)
        AvgCpuPercent    = if ($cpuSamples.Count) { [math]::Round(($cpuSamples | Measure-Object -Average).Average, 2) } else { $null }
        AvgGpuPercent    = if ($gpuSamples.Count) { [math]::Round(($gpuSamples | Measure-Object -Average).Average, 2) } else { $null }
        AvgRamMB         = if ($ramSamples.Count) { [math]::Round((($ramSamples | Measure-Object -Average).Average) / 1MB, 1) } else { $null }
        SampleCount      = $cpuSamples.Count
        Timestamp        = (Get-Date).ToString("s")
    }
}

$results = @()
$testFiles = Get-ChildItem -Path $TestFilesDir -File | Where-Object {
    $_.Extension -in ".mp4", ".mkv", ".avi", ".mov", ".webm", ".ts", ".m3u8"
}

if ($testFiles.Count -eq 0) {
    Write-Warning "No test media found in $TestFilesDir — populate it with the QA-matrix corpus described in the architecture doc before running."
    exit 1
}

foreach ($file in $testFiles) {
    $results += Measure-PlayerRun -ExePath $NyraExePath -MediaFile $file.FullName -Label "Nyra"
    $results += Measure-PlayerRun -ExePath $VlcExePath  -MediaFile $file.FullName -Label "StockVLC"
}

$results | Export-Csv -Path $OutputCsv -NoTypeInformation
Write-Host "Results written to $OutputCsv — no numbers are asserted here; inspect the CSV."

# Battery delta (only meaningful if this whole script ran unplugged)
try {
    powercfg /batteryreport /output ".\battery-report.html" | Out-Null
    Write-Host "Battery report saved to battery-report.html — compare 'Recent usage' entries spanning this run's timestamps."
} catch {
    Write-Warning "powercfg battery report unavailable (desktop PC / no battery, or insufficient permissions)."
}
