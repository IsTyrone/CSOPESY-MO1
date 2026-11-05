param([string]$Executable = 'csopesy.exe')
$ErrorActionPreference = 'Stop'

# Each emulator gets its own config/report directory; the user's config is untouched.
function Start-Scenario($name, $settings) {
    $folder = Join-Path ([IO.Path]::GetTempPath()) ('csopesy-' + [guid]::NewGuid().ToString('N'))
    [void][IO.Directory]::CreateDirectory($folder)
    $settings | Set-Content (Join-Path $folder 'config.txt') -Encoding ASCII
    $info = New-Object Diagnostics.ProcessStartInfo
    $info.FileName = Join-Path $PSScriptRoot $Executable
    $info.WorkingDirectory = $folder
    $info.UseShellExecute = $false
    $info.CreateNoWindow = $true
    $info.RedirectStandardInput = $true
    $info.RedirectStandardOutput = $true
    $info.RedirectStandardError = $true
    $info.EnvironmentVariables.Remove('CSOPESY_TICK_MS')
    $info.EnvironmentVariables.Remove('CSOPESY_SEED')
    $process = New-Object Diagnostics.Process
    $process.StartInfo = $info
    [void]$process.Start()
    $out = $process.StandardOutput.ReadToEndAsync()
    $err = $process.StandardError.ReadToEndAsync()
    $process.StandardInput.AutoFlush = $true
    $process.StandardInput.WriteLine('initialize')
    $process.StandardInput.WriteLine('scheduler-test')
    [pscustomobject]@{ Name=$name; Process=$process; Out=$out; Err=$err; Folder=$folder }
}
function Send-Command($scenario, $command) { $scenario.Process.StandardInput.WriteLine($command) }
function Finish-Scenario($scenario) {
    Send-Command $scenario 'exit'
    $scenario.Process.StandardInput.Close()
}
function Get-Reports($output) {
    [regex]::Matches($output, '(?ms)CPU utilization: (\d+)%.*?(?=root>|\z)')
}

$cases = @()
try {
    $drain = Start-Scenario 'drain' @('num-cpu 8','scheduler "rr"','quantum-cycles 1','batch-process-freq 1','min-ins 100','max-ins 100','delay-per-exec 0')
    $cases += $drain
    $idle = Start-Scenario 'idle' @('num-cpu 1','scheduler "rr"','quantum-cycles 10','batch-process-freq 20','min-ins 30','max-ins 45','delay-per-exec 3')
    $cases += $idle
    $load = Start-Scenario 'load' @('num-cpu 16','scheduler "rr"','quantum-cycles 5','batch-process-freq 1','min-ins 5000','max-ins 5000','delay-per-exec 3')
    $cases += $load
    $clock = [Diagnostics.Stopwatch]::StartNew()
    $drainStopped = $false
    $idleDone = $false
    $nextLoadSample = 10
    $loadSamples = 0
    while ($clock.Elapsed.TotalSeconds -lt 40) {
        Start-Sleep -Milliseconds 100
        $seconds = $clock.Elapsed.TotalSeconds
        if (!$drainStopped -and $seconds -ge 10) {
            Send-Command $drain 'scheduler-stop'
            $drainStopped = $true
            $drainStoppedAt = $seconds
        }
        if (!$idleDone) {
            Send-Command $idle 'screen -ls'
            if ($seconds -ge 15) {
                Send-Command $idle 'scheduler-stop'
                Finish-Scenario $idle
                $idleDone = $true
            }
        }
        if ($loadSamples -lt 5 -and $seconds -ge $nextLoadSample) {
            Send-Command $load 'screen -ls'
            Write-Output ('Load sample {0} at {1:F3}s' -f ($loadSamples + 1), $seconds)
            $loadSamples++
            # Schedule relative to this sample to ensure at least 5 s between commands.
            $nextLoadSample = $seconds + 5
            if ($loadSamples -eq 5) {
                Send-Command $load 'scheduler-stop'
                Send-Command $load 'report-util'
                Finish-Scenario $load
            }
        }
    }
    # Account for polling drift so draining always lasts at least 30 seconds.
    $remaining = 30 - ($clock.Elapsed.TotalSeconds - $drainStoppedAt)
    if ($remaining -gt 0) { Start-Sleep -Milliseconds ([int][Math]::Ceiling($remaining * 1000)) }
    Send-Command $drain 'screen -ls'
    Finish-Scenario $drain

    foreach ($case in $cases) {
        if (!$case.Process.WaitForExit(10000)) { throw "$($case.Name) did not exit" }
        $case.Out.Result | Set-Content (Join-Path $PSScriptRoot "scheduler-$($case.Name)-test-output.txt") -Encoding UTF8
        if ($case.Process.ExitCode -ne 0) { throw $case.Err.Result }
    }
    $drainReport = @(Get-Reports $drain.Out.Result)
    if ($drainReport.Count -ne 1 -or $drainReport[0].Groups[1].Value -ne '0' -or
        $drainReport[0].Value -match '(?m)^p\d+\s+.*Core:' -or
        $drainReport[0].Value -notmatch '(?m)^p\d+\s+.*Finished') { throw 'Drain case failed' }
    Write-Output 'PASS drain: 0% CPU, no active processes, all generated processes finished.'

    $idleReports = @(Get-Reports $idle.Out.Result)
    $busy = @($idleReports | Where-Object { $_.Groups[1].Value -eq '100' }).Count
    $idleFinished = @($idleReports | Where-Object {
        $_.Groups[1].Value -eq '0' -and $_.Value -match 'Running processes:\r?\n\(none\)' -and
        $_.Value -match '(?m)^p\d+\s+.*Finished'
    }).Count
    if (!$busy -or !$idleFinished) { throw 'Idle case failed' }
    Write-Output "PASS idle: $busy busy snapshots; $idleFinished idle snapshots after completion."

    $loadReports = @(Get-Reports $load.Out.Result)
    $report = Get-Content (Join-Path $load.Folder 'csopesy-log.txt') -Raw
    $report | Set-Content (Join-Path $PSScriptRoot 'scheduler-load-test-report.txt') -Encoding UTF8
    $finished = [regex]::Matches($report, '(?m)^p\d+\s+.*Finished').Count
    $utilizations = @($loadReports | ForEach-Object { $_.Groups[1].Value })
    if ($loadReports.Count -ne 5 -or @($utilizations | Where-Object { $_ -ne '100' }).Count -or
        $report -notmatch 'CPU utilization: 100%' -or $finished -ge 10 -or
        $report -notmatch '(?m)^p\d+\s+.*Core:') { throw 'Load case failed' }
    Write-Output "PASS load: snapshots $($utilizations -join ', ')%; saved report 100%; finished $finished."
    Write-Output 'ALL THREE REAL-TIME SCENARIOS PASSED.'
} finally {
    foreach ($case in $cases) {
        if (!$case.Process.HasExited) { $case.Process.Kill() }
        $case.Process.Dispose()
    }
}
