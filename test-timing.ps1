param([string]$Executable = 'csopesy.exe')

$ErrorActionPreference = 'Stop'
$expected = @{
    'num-cpu' = '8'; 'scheduler' = 'rr'; 'quantum-cycles' = '1'
    'batch-process-freq' = '1'; 'min-ins' = '100'; 'max-ins' = '100'
    'delay-per-exec' = '0'
}
$settings = @{}
foreach ($line in Get-Content (Join-Path $PSScriptRoot 'config.txt')) {
    $clean = ($line -split '#', 2)[0].Trim()
    if ($clean) {
        $parts = $clean -split '\s+', 2
        if ($parts.Count -eq 2) { $settings[$parts[0]] = $parts[1].Trim('"') }
    }
}
foreach ($key in $expected.Keys) {
    if ($settings[$key] -ne $expected[$key]) {
        throw "This acceptance test requires config.txt setting: $key $($expected[$key])"
    }
}
$info = New-Object System.Diagnostics.ProcessStartInfo
$info.FileName = Join-Path $PSScriptRoot $Executable
$info.WorkingDirectory = $PSScriptRoot
$info.UseShellExecute = $false
$info.CreateNoWindow = $true
$info.RedirectStandardInput = $true
$info.RedirectStandardOutput = $true
$info.RedirectStandardError = $true
# Exercise the defaults, independently of the caller's timing overrides.
$info.EnvironmentVariables.Remove('CSOPESY_TICK_MS')
$info.EnvironmentVariables.Remove('CSOPESY_SEED')
$process = New-Object System.Diagnostics.Process
$process.StartInfo = $info
try {
    [void]$process.Start()
    $stdout = $process.StandardOutput.ReadToEndAsync()
    $stderr = $process.StandardError.ReadToEndAsync()
    $process.StandardInput.AutoFlush = $true
    $process.StandardInput.WriteLine('initialize')
    $process.StandardInput.WriteLine('scheduler-test')
    $clock = [System.Diagnostics.Stopwatch]::StartNew()
    Start-Sleep -Seconds 10
    $process.StandardInput.WriteLine('scheduler-stop')
    $stoppedAt = $clock.Elapsed.TotalSeconds
    Start-Sleep -Seconds 30
    $process.StandardInput.WriteLine('screen -ls')
    $reportedAt = $clock.Elapsed.TotalSeconds
    $process.StandardInput.WriteLine('exit')
    $process.StandardInput.Close()
    if (!$process.WaitForExit(10000)) { throw 'Emulator did not exit.' }
    $output = $stdout.Result
    $output | Set-Content (Join-Path $PSScriptRoot 'scheduler-timing-test-output.txt') -Encoding UTF8
    Write-Output ('Generation: {0:F3}s; drain wait: {1:F3}s' -f $stoppedAt, ($reportedAt - $stoppedAt))
    $active = @([regex]::Matches($output, '(?m)^p\d+\s+.*Core:')).Count
    $finished = @([regex]::Matches($output, '(?m)^p\d+\s+.*Finished')).Count
    Write-Output "Active: $active; finished: $finished"
    if ($process.ExitCode -ne 0) { throw $stderr.Result }
    if ($output -notmatch 'Scheduler started \(tick = 1 ms\)' -or
        $output -notmatch 'CPU utilization: 0%' -or
        $active -ne 0 -or $finished -eq 0) {
        throw 'Timing test failed. See scheduler-timing-test-output.txt.'
    }
    Write-Output 'TIMING TEST PASSED: 0% utilization and all generated processes finished.'
} finally {
    if ($process.Id -and !$process.HasExited) { $process.Kill() }
    $process.Dispose()
}
