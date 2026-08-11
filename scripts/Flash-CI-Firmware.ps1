[CmdletBinding()]
param(
    [string]$Port = '',
    [switch]$ListOnly,
    [switch]$SelfTest
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$Repo = 'waveshareteam/ESP32-P4-WIFI6-Touch-LCD-3.5'
$Workflow = 'esp-idf.yml'
$DefaultStartIndex = 1
$ExpectedTarget = 'esp32p4'
$ExpectedFramework = 'ESP-IDF'
$ExpectedBaud = 460800
$FlashCapacity = 16MB
$ProjectNames = @(
    '01_HowToCreateProject',
    '02_HelloWorld',
    '03_i2c_tools',
    '04_wifistation',
    '05_sdmmc',
    '06_I2SCodec',
    '07_Displaycolorbar',
    '08_lvgl_demo_v9',
    '09_video_lcd_display',
    '10_mp4_player',
    '11_esp_brookesia_phone',
    '12_esp32-p4-eye'
)
$Versions = @('v5.5.5', 'v6.0.2')
$Items = @()
$itemIndex = 1
foreach ($projectName in $ProjectNames) {
    foreach ($version in $Versions) {
        $Items += [pscustomobject]@{
            Index = $itemIndex
            Workflow = $Workflow
            Name = $projectName
            Framework = $ExpectedFramework
            Version = $version
            SourceProject = "examples/esp-idf/$projectName"
            Artifact = "esp-idf-$version-$projectName-esp32p4-<ci-build-sha>"
            BuildSha = '<ci-build-sha>'
        }
        $itemIndex++
    }
}

function Test-Port([string]$Value) {
    return $Value -match '^COM\d+$'
}

function Get-NextProgress([int]$CurrentIndex, [int[]]$ConfirmedIndexes, [int]$ItemCount) {
    if ($ItemCount -lt 1 -or $CurrentIndex -lt 1 -or $CurrentIndex -gt $ItemCount) {
        throw 'Progress indexes must be within the item range.'
    }
    $confirmed = @(
        $ConfirmedIndexes + $CurrentIndex |
            Where-Object { $_ -ge 1 -and $_ -le $ItemCount } |
            Sort-Object -Unique
    )
    return [pscustomobject]@{
        CurrentIndex = if ($CurrentIndex -eq $ItemCount) { $CurrentIndex } else { $CurrentIndex + 1 }
        ConfirmedIndexes = $confirmed
        Completed = $CurrentIndex -eq $ItemCount
    }
}

function Get-StateForBuild(
    $Saved,
    [string]$ExpectedFinalSha,
    [string]$ExpectedBuildSha,
    [string]$DefaultPort
) {
    if (
        -not $Saved -or
        -not $Saved.PSObject.Properties['FinalSha'] -or
        -not $Saved.PSObject.Properties['BuildSha'] -or
        -not $Saved.PSObject.Properties['CurrentIndex'] -or
        -not $Saved.PSObject.Properties['ConfirmedIndexes'] -or
        [string]$Saved.FinalSha -ne $ExpectedFinalSha -or
        [string]$Saved.BuildSha -ne $ExpectedBuildSha
    ) {
        return [pscustomobject]@{ CurrentIndex = $DefaultStartIndex; ConfirmedIndexes = @(); Port = $DefaultPort }
    }
    $index = [int]$Saved.CurrentIndex
    if ($index -lt 1 -or $index -gt $Items.Count) {
        throw "Saved CurrentIndex is outside 1..$($Items.Count)."
    }
    return [pscustomobject]@{
        CurrentIndex = $index
        ConfirmedIndexes = @(
            $Saved.ConfirmedIndexes |
                ForEach-Object { [int]$_ } |
                Where-Object { $_ -ge 1 -and $_ -le $Items.Count } |
                Sort-Object -Unique
        )
        Port = $DefaultPort
    }
}

function Test-RelativePackagePath([string]$PackageRoot, [string]$RelativePath) {
    if ([string]::IsNullOrWhiteSpace($RelativePath) -or [System.IO.Path]::IsPathRooted($RelativePath)) {
        return $false
    }
    $root = [System.IO.Path]::GetFullPath($PackageRoot).TrimEnd(
        [System.IO.Path]::DirectorySeparatorChar,
        [System.IO.Path]::AltDirectorySeparatorChar
    ) + [System.IO.Path]::DirectorySeparatorChar
    $candidate = [System.IO.Path]::GetFullPath((Join-Path $PackageRoot $RelativePath))
    return $candidate.StartsWith($root, [System.StringComparison]::OrdinalIgnoreCase)
}

function Get-FileSha256([string]$Path) {
    $stream = $null
    $algorithm = $null
    try {
        $stream = [System.IO.File]::OpenRead($Path)
        $algorithm = [System.Security.Cryptography.SHA256]::Create()
        return [System.BitConverter]::ToString($algorithm.ComputeHash($stream)).Replace('-', '').ToLowerInvariant()
    }
    finally {
        if ($null -ne $stream) { $stream.Dispose() }
        if ($null -ne $algorithm) { $algorithm.Dispose() }
    }
}

function Get-ArtifactBuildSha([string]$ArtifactName, [string]$ExpectedPrefix) {
    $pattern = '^' + [regex]::Escape($ExpectedPrefix) + '([0-9a-fA-F]{40})$'
    $match = [regex]::Match($ArtifactName, $pattern)
    if (-not $match.Success) {
        throw "Artifact name does not match $ExpectedPrefix<ci-build-sha>."
    }
    return $match.Groups[1].Value.ToLowerInvariant()
}

function Test-PackageManifest([string]$PackageDir, $Item, [string]$ArtifactSha) {
    $manifestPath = Join-Path $PackageDir 'manifest.json'
    if (-not (Test-Path -LiteralPath $manifestPath -PathType Leaf)) {
        throw 'Package manifest.json is missing.'
    }
    $manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
    if (
        [string]$manifest.name -ne $Item.Name -or
        [string]$manifest.framework -ine $Item.Framework -or
        [string]$manifest.framework_version -ne $Item.Version -or
        [string]$manifest.target -ne $ExpectedTarget -or
        [string]$manifest.project_path -ne $Item.SourceProject -or
        [string]$manifest.git_sha -ne $ArtifactSha
    ) {
        throw 'Package manifest identity does not match the selected ESP32-P4 CI artifact.'
    }
    if ([int64]$manifest.baud -ne $ExpectedBaud -or @($manifest.files).Count -lt 1) {
        throw 'Package manifest flash metadata is incomplete or unsafe.'
    }

    $plan = @()
    $offsets = @{}
    foreach ($file in @($manifest.files)) {
        $relativePath = [string]$file.path
        if (
            -not (Test-RelativePackagePath $PackageDir $relativePath) -or
            [string]$file.sha256 -notmatch '^[0-9a-fA-F]{64}$' -or
            [int64]$file.size -le 0
        ) {
            throw "Manifest file metadata is unsafe: $relativePath"
        }
        $fullPath = [System.IO.Path]::GetFullPath((Join-Path $PackageDir $relativePath))
        if (-not (Test-Path -LiteralPath $fullPath -PathType Leaf)) {
            throw "Manifest file is missing: $relativePath"
        }
        $actualHash = Get-FileSha256 $fullPath
        $expectedHash = ([string]$file.sha256).ToLowerInvariant()
        $actualSize = [int64](Get-Item -LiteralPath $fullPath).Length
        if ($actualHash -ne $expectedHash -or $actualSize -ne [int64]$file.size) {
            throw "Manifest checksum or size verification failed: $relativePath"
        }
        if ([string]$file.offset -notmatch '^0x[0-9a-fA-F]+$') {
            throw "Manifest flash offset is invalid: $relativePath"
        }
        $offset = [Convert]::ToInt64(([string]$file.offset).Substring(2), 16)
        if ($offsets.ContainsKey($offset) -or $offset + [int64]$file.size -gt $FlashCapacity) {
            throw "Manifest flash range is unsafe: $relativePath"
        }
        $offsets[$offset] = $true
        $plan += [pscustomobject]@{ Offset = $offset; Size = [int64]$file.size; Path = $fullPath }
    }
    if ($plan.Count -lt 1) {
        throw 'Package manifest contains no flashable files.'
    }
    $orderedPlan = @($plan | Sort-Object Offset)
    for ($index = 1; $index -lt $orderedPlan.Count; ++$index) {
        $previous = $orderedPlan[$index - 1]
        if ($previous.Offset + $previous.Size -gt $orderedPlan[$index].Offset) {
            throw 'Package manifest contains overlapping flash ranges.'
        }
    }
    return $orderedPlan
}

function Invoke-SelfTest {
    if ($Items.Count -ne ($ProjectNames.Count * $Versions.Count) -or $Items.Count -ne 24) {
        throw 'SelfTest expected 24 ESP-IDF matrix items.'
    }
    $current = $DefaultStartIndex
    $confirmed = @()
    $transitions = 0
    while ($current -lt $Items.Count) {
        $next = Get-NextProgress $current $confirmed $Items.Count
        if ($next.Completed -or $next.CurrentIndex -ne ($current + 1)) {
            throw 'SelfTest expected one-item progress.'
        }
        $current = $next.CurrentIndex
        $confirmed = @($next.ConfirmedIndexes)
        $transitions++
    }
    $last = Get-NextProgress $current $confirmed $Items.Count
    if (-not $last.Completed -or @($last.ConfirmedIndexes).Count -ne $Items.Count) {
        throw 'SelfTest did not complete every item.'
    }
    $reset = Get-StateForBuild ([pscustomobject]@{
        FinalSha = 'different'
        BuildSha = 'expected-build'
        CurrentIndex = 4
        ConfirmedIndexes = @(1, 2, 3)
        Port = 'ignored'
    }) 'expected' 'expected-build' ''
    if ($reset.CurrentIndex -ne 1 -or @($reset.ConfirmedIndexes).Count -ne 0) {
        throw 'SelfTest did not reset state for a new SHA.'
    }
    $buildReset = Get-StateForBuild ([pscustomobject]@{
        FinalSha = 'expected'
        BuildSha = 'different-build'
        CurrentIndex = 4
        ConfirmedIndexes = @(1, 2, 3)
        Port = 'ignored'
    }) 'expected' 'expected-build' ''
    if ($buildReset.CurrentIndex -ne 1 -or @($buildReset.ConfirmedIndexes).Count -ne 0) {
        throw 'SelfTest did not reset state for a new CI build SHA.'
    }
    if (
        (Test-RelativePackagePath 'C:\package' '..\escape.bin') -or
        (Test-RelativePackagePath 'C:\package' 'C:\escape.bin') -or
        -not (Test-RelativePackagePath 'C:\package' 'bin\app.bin')
    ) {
        throw 'SelfTest relative manifest path validation failed.'
    }
    $syntheticBuildSha = 'fedcba9876543210fedcba9876543210fedcba98'
    $syntheticPrefix = 'esp-idf-v5.5.5-example-esp32p4-'
    if ((Get-ArtifactBuildSha ($syntheticPrefix + $syntheticBuildSha) $syntheticPrefix) -ne $syntheticBuildSha) {
        throw 'SelfTest did not resolve the CI build SHA from an artifact name.'
    }
    $rejectedArtifactName = $false
    try { [void](Get-ArtifactBuildSha ($syntheticPrefix + 'not-a-sha') $syntheticPrefix) } catch { $rejectedArtifactName = $true }
    if (-not $rejectedArtifactName) {
        throw 'SelfTest did not reject an artifact without a full CI build SHA.'
    }

    $tempRoot = Join-Path ([System.IO.Path]::GetTempPath()) ('waveshare-flasher-' + [guid]::NewGuid().ToString('N'))
    try {
        $binDir = Join-Path $tempRoot 'bin'
        New-Item -ItemType Directory -Path $binDir -Force | Out-Null
        $binPath = Join-Path $binDir 'app.bin'
        [System.IO.File]::WriteAllBytes($binPath, [byte[]](1, 2, 3, 4))
        $item = $Items[0]
        $sha = '0123456789abcdef0123456789abcdef01234567'
        $manifest = [ordered]@{
            name = $item.Name
            framework = $item.Framework
            framework_version = $item.Version
            target = $ExpectedTarget
            project_path = $item.SourceProject
            git_sha = $sha
            timestamp_utc = '2026-01-01T00:00:00Z'
            baud = $ExpectedBaud
            files = @([ordered]@{
                offset = '0x10000'
                path = 'bin/app.bin'
                size = 4
                sha256 = Get-FileSha256 $binPath
            })
            flash_command = 'synthetic self-test only'
        }
        $manifestPath = Join-Path $tempRoot 'manifest.json'
        $manifest | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $manifestPath -Encoding UTF8
        $plan = @(Test-PackageManifest $tempRoot $item $sha)
        if ($plan.Count -ne 1 -or $plan[0].Offset -ne 0x10000) {
            throw 'SelfTest valid manifest was not accepted.'
        }
        $manifest.target = 'esp32c6'
        $manifest | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $manifestPath -Encoding UTF8
        $rejectedC6 = $false
        try { [void](Test-PackageManifest $tempRoot $item $sha) } catch { $rejectedC6 = $true }
        if (-not $rejectedC6) {
            throw 'SelfTest did not reject an ESP32-C6 manifest.'
        }
    }
    finally {
        if (Test-Path -LiteralPath $tempRoot) {
            Remove-Item -LiteralPath $tempRoot -Recurse -Force
        }
    }
    Write-Output 'SELF_TEST_OK items=24 transitions=23 completed=24 target=esp32p4 c6Rejected=true artifactShaBound=true'
}

if ($SelfTest) {
    Invoke-SelfTest
    return
}

if ($ListOnly) {
    Write-Output 'finalSHA=resolved-at-runtime'
    Write-Output 'defaultPort=auto-detect-at-runtime'
    Write-Output "startIndex=$DefaultStartIndex"
    Write-Output 'target=esp32p4'
    Write-Output "baud=$ExpectedBaud"
    foreach ($item in $Items) {
        Write-Output ('{0}: workflow={1} run=resolved-at-runtime artifact={2} project={3}' -f $item.Index, $item.Workflow, $item.Artifact, $item.SourceProject)
    }
    return
}

function Resolve-DefaultPort {
    $pnpPorts = @(
        Get-CimInstance Win32_PnPEntity -ErrorAction SilentlyContinue |
            Where-Object { $_.PNPDeviceID -match 'VID_303A&PID_1001' -and $_.Name -match '\(COM\d+\)' } |
            ForEach-Object { [regex]::Match($_.Name, '\((COM\d+)\)').Groups[1].Value } |
            Sort-Object -Unique
    )
    if ($pnpPorts.Count -eq 1) { return $pnpPorts[0] }
    throw 'Unable to identify exactly one ESP32-P4 USB serial port; pass -Port COMx.'
}

$RepoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$StateRoot = Join-Path $env:LOCALAPPDATA 'Waveshare\ESP32-P4-WIFI6-Touch-LCD-3.5\ci-firmware'
$StatePath = Join-Path $StateRoot 'state-v1.json'

function Resolve-Executable([string]$Name, [string[]]$Fallbacks) {
    $command = Get-Command $Name -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($command -and $command.Source) { return $command.Source }
    foreach ($candidate in $Fallbacks) {
        if (Test-Path -LiteralPath $candidate -PathType Leaf) { return $candidate }
    }
    throw "$Name was not found on PATH or in the supported fallback locations."
}

function Resolve-Git {
    return Resolve-Executable 'git' @(
        (Join-Path ${env:ProgramFiles} 'Git\cmd\git.exe'),
        (Join-Path ${env:ProgramFiles} 'Git\bin\git.exe'),
        'C:\Git\cmd\git.exe',
        'D:\Git\cmd\git.exe'
    )
}

function Resolve-Gh {
    return Resolve-Executable 'gh' @(
        (Join-Path ${env:ProgramFiles} 'GitHub CLI\gh.exe'),
        (Join-Path ${env:ProgramFiles} 'GitHub CLI\bin\gh.exe')
    )
}

function Resolve-PythonWithEsptool {
    $command = Get-Command python -ErrorAction SilentlyContinue | Select-Object -First 1
    $candidates = @()
    if ($command -and $command.Source) { $candidates += $command.Source }
    foreach ($programFiles in (@($env:ProgramFiles, ${env:ProgramFiles(x86)}) | Where-Object { $_ })) {
        $candidates += @(
            Get-ChildItem -Path (Join-Path $programFiles 'Python*') -File -Filter python.exe -ErrorAction SilentlyContinue |
                ForEach-Object FullName
        )
    }
    foreach ($root in @((Join-Path $env:USERPROFILE '.espressif\python_env'), 'C:\Espressif', 'D:\espressif')) {
        if (Test-Path -LiteralPath $root) {
            $candidates += @(
                Get-ChildItem -LiteralPath $root -Recurse -File -Filter python.exe -ErrorAction SilentlyContinue |
                    Where-Object { $_.FullName -match '[\\/]python_env[\\/].+[\\/]Scripts[\\/]python\.exe$' } |
                    ForEach-Object FullName
            )
        }
    }
    foreach ($candidate in @($candidates | Select-Object -Unique)) {
        $savedErrorActionPreference = $ErrorActionPreference
        $probeExitCode = $null
        try {
            $ErrorActionPreference = 'Continue'
            & $candidate -c 'import esptool' *> $null
            $probeExitCode = $LASTEXITCODE
        }
        catch { $probeExitCode = -1 }
        finally { $ErrorActionPreference = $savedErrorActionPreference }
        if ($probeExitCode -eq 0) { return $candidate }
    }
    throw 'No Python interpreter with esptool was found.'
}

function Resolve-EsptoolWriteOperation([string]$PythonExe) {
    foreach ($operation in @('write-flash', 'write_flash')) {
        $savedErrorActionPreference = $ErrorActionPreference
        $probeExitCode = $null
        try {
            $ErrorActionPreference = 'Continue'
            & $PythonExe -m esptool $operation --help *> $null
            $probeExitCode = $LASTEXITCODE
        }
        catch { $probeExitCode = -1 }
        finally { $ErrorActionPreference = $savedErrorActionPreference }
        if ($probeExitCode -eq 0) { return $operation }
    }
    throw 'The installed esptool does not expose a supported write-flash operation.'
}

function Resolve-FinalSha([string]$GitExe) {
    $sha = (& $GitExe -C $RepoRoot rev-parse HEAD 2>&1 | Out-String).Trim()
    if ($LASTEXITCODE -ne 0 -or $sha -notmatch '^[0-9a-fA-F]{40}$') {
        throw 'Unable to resolve a full local git HEAD SHA.'
    }
    return $sha.ToLowerInvariant()
}

function Assert-CleanWorktree([string]$GitExe) {
    $status = (& $GitExe -C $RepoRoot status --porcelain=v1 --untracked-files=all 2>&1 | Out-String)
    if ($LASTEXITCODE -ne 0) { throw 'Unable to determine whether the working tree is clean.' }
    if (-not [string]::IsNullOrWhiteSpace($status)) {
        throw 'Refusing to continue: the working tree has staged, unstaged, or untracked changes.'
    }
}

function Resolve-CurrentBranch([string]$GitExe) {
    $branch = (& $GitExe -C $RepoRoot symbolic-ref --quiet --short HEAD 2>&1 | Out-String).Trim()
    if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($branch)) {
        throw 'Refusing to continue: check out a non-detached branch first.'
    }
    return $branch
}

function Assert-ReadyPullRequest([string]$GhExe, [string]$Branch, [string]$FinalSha) {
    $raw = (& $GhExe pr list --repo $Repo --head $Branch --state open --limit 2 --json number,state,isDraft,headRefName,headRefOid 2>&1 | Out-String)
    if ($LASTEXITCODE -ne 0) { throw 'Unable to query the open pull request for the current branch.' }
    $pullRequests = @($raw | ConvertFrom-Json)
    if ($pullRequests.Count -ne 1) {
        throw 'Refusing to continue: the current branch must have exactly one open pull request.'
    }
    $pullRequest = $pullRequests[0]
    if (
        [string]$pullRequest.state -ine 'OPEN' -or
        [bool]$pullRequest.isDraft -or
        [string]$pullRequest.headRefName -ne $Branch
    ) {
        throw 'Refusing to continue: the pull request must be open, ready for review, and belong to the current branch.'
    }
    $headRefOid = [string]$pullRequest.headRefOid
    if (
        $headRefOid -notmatch '^[0-9a-fA-F]{40}$' -or
        -not [string]::Equals($headRefOid, $FinalSha, [System.StringComparison]::OrdinalIgnoreCase)
    ) {
        throw 'Refusing to continue: the pull request head must be the complete local HEAD SHA.'
    }
}

function Resolve-ArtifactRuns([string]$GhExe, [string]$FinalSha) {
    $raw = (& $GhExe run list --repo $Repo --workflow $Workflow --commit $FinalSha --limit 20 --json databaseId,headSha,createdAt,status,conclusion 2>&1 | Out-String)
    if ($LASTEXITCODE -ne 0) { throw "Unable to list $Workflow runs: $raw" }
    $runs = @(
        $raw | ConvertFrom-Json |
            Where-Object { [string]$_.headSha -eq $FinalSha } |
            Sort-Object createdAt -Descending
    )
    if ($runs.Count -lt 1) {
        throw "No $Workflow run exists for local HEAD $FinalSha; refusing to use another artifact."
    }
    $latestRun = $runs[0]
    if ([string]$latestRun.status -ine 'completed' -or [string]$latestRun.conclusion -ine 'success') {
        throw "The latest $Workflow run for local HEAD $FinalSha is not completed successfully."
    }
    $run = [string]$latestRun.databaseId
    $artifactRaw = (& $GhExe api "repos/$Repo/actions/runs/$run/artifacts?per_page=100" 2>&1 | Out-String)
    if ($LASTEXITCODE -ne 0) { throw "Unable to list artifacts for run ${run}: $artifactRaw" }
    $artifactResponse = $artifactRaw | ConvertFrom-Json
    $availableArtifacts = @($artifactResponse.artifacts | Where-Object { -not [bool]$_.expired })
    foreach ($item in $Items) {
        $prefix = "esp-idf-$($item.Version)-$($item.Name)-esp32p4-"
        $namePattern = '^' + [regex]::Escape($prefix) + '([0-9a-fA-F]{40})$'
        $matches = @($availableArtifacts | Where-Object { [string]$_.name -match $namePattern })
        if ($matches.Count -ne 1) {
            throw "Expected exactly one non-expired artifact matching $prefix<ci-build-sha> in run $run."
        }
        $artifactName = [string]$matches[0].name
        $item.Artifact = $artifactName
        $item.BuildSha = Get-ArtifactBuildSha $artifactName $prefix
        $item | Add-Member -NotePropertyName Run -NotePropertyValue $run -Force
    }
    $buildShas = @($Items.BuildSha | Sort-Object -Unique)
    if ($buildShas.Count -ne 1) {
        throw "Artifacts in run $run do not share one CI build SHA."
    }
}

function Ensure-StateRoot {
    if (-not (Test-Path -LiteralPath $StateRoot)) {
        New-Item -ItemType Directory -Path $StateRoot | Out-Null
    }
}

function Read-State([string]$FinalSha, [string]$BuildSha) {
    $saved = if (Test-Path -LiteralPath $StatePath) {
        Get-Content -LiteralPath $StatePath -Raw | ConvertFrom-Json
    } else {
        $null
    }
    return Get-StateForBuild $saved $FinalSha $BuildSha $Port
}

function Save-State(
    [int]$CurrentIndex,
    [int[]]$ConfirmedIndexes,
    [string]$SavedPort,
    [string]$FinalSha,
    [string]$BuildSha
) {
    Ensure-StateRoot
    [pscustomobject]@{
        CurrentIndex = $CurrentIndex
        ConfirmedIndexes = @($ConfirmedIndexes | Sort-Object -Unique)
        Port = $SavedPort
        UpdatedAt = (Get-Date).ToString('o')
        Repository = $Repo
        FinalSha = $FinalSha
        BuildSha = $BuildSha
    } | ConvertTo-Json | Set-Content -LiteralPath $StatePath -Encoding UTF8
}

function New-RunPaths {
    Ensure-StateRoot
    $stamp = Get-Date -Format 'yyyyMMdd-HHmmss-fff'
    $downloadRoot = Join-Path $StateRoot 'downloads'
    $logRoot = Join-Path $StateRoot 'logs'
    foreach ($dir in @($downloadRoot, $logRoot)) {
        if (-not (Test-Path -LiteralPath $dir)) { New-Item -ItemType Directory -Path $dir | Out-Null }
    }
    $downloadDir = Join-Path $downloadRoot $stamp
    $logPath = Join-Path $logRoot ($stamp + '.log')
    if ((Test-Path -LiteralPath $downloadDir) -or (Test-Path -LiteralPath $logPath)) {
        throw "Timestamp collision at $stamp; no existing files were changed."
    }
    New-Item -ItemType Directory -Path $downloadDir | Out-Null
    New-Item -ItemType File -Path $logPath | Out-Null
    return [pscustomobject]@{ DownloadDir = $downloadDir; LogPath = $logPath }
}

function Add-RunLog([string]$Path, [string]$Text) {
    Add-Content -LiteralPath $Path -Value $Text -Encoding UTF8
}

function Find-PackageDirectory([string]$DownloadDir) {
    $zips = @(Get-ChildItem -LiteralPath $DownloadDir -Recurse -File -Filter '*.zip')
    foreach ($zip in $zips) {
        $destination = Join-Path $zip.DirectoryName ($zip.BaseName + '-unzipped')
        if (Test-Path -LiteralPath $destination) {
            throw "Refusing to overwrite extraction directory: $destination"
        }
        Expand-Archive -LiteralPath $zip.FullName -DestinationPath $destination -ErrorAction Stop
    }
    $manifests = @(Get-ChildItem -LiteralPath $DownloadDir -Recurse -File -Filter 'manifest.json')
    if ($manifests.Count -ne 1) {
        throw 'Expected exactly one manifest.json in the downloaded artifact.'
    }
    return $manifests[0].DirectoryName
}

function Invoke-CurrentFlash(
    $Item,
    [string]$SelectedPort,
    [string]$GhExe,
    [string]$PythonExe,
    [string]$WriteOperation,
    [string]$FinalSha
) {
    $paths = New-RunPaths
    Add-RunLog $paths.LogPath "finalSHA=$FinalSha buildSHA=$($Item.BuildSha) index=$($Item.Index) artifact=$($Item.Artifact) run=$($Item.Run) port=$SelectedPort"
    $downloadOutput = (& $GhExe run download $Item.Run --repo $Repo --name $Item.Artifact --dir $paths.DownloadDir 2>&1 | Out-String)
    $downloadExit = $LASTEXITCODE
    Add-RunLog $paths.LogPath $downloadOutput
    if ($downloadExit -ne 0) {
        throw "Artifact download failed with exit code $downloadExit. Log: $($paths.LogPath)"
    }
    $packageDir = Find-PackageDirectory $paths.DownloadDir
    $plan = Test-PackageManifest $packageDir $Item $Item.BuildSha
    $flashArguments = @(
        '-m', 'esptool', '--port', $SelectedPort, '--chip', $ExpectedTarget,
        '--baud', [string]$ExpectedBaud, $WriteOperation
    )
    foreach ($entry in $plan) {
        $flashArguments += ('0x{0:X}' -f $entry.Offset)
        $flashArguments += $entry.Path
    }
    $flashOutput = (& $PythonExe @flashArguments 2>&1 | Out-String)
    $flashExit = $LASTEXITCODE
    Add-RunLog $paths.LogPath $flashOutput
    $verified = ($flashExit -eq 0) -and $flashOutput.Contains('Hash of data verified')
    return [pscustomobject]@{
        Success = $verified
        Output = $flashOutput
        LogPath = $paths.LogPath
        Detail = if ($verified) {
            'Flash completed and Hash of data verified was found.'
        } else {
            'Flash did not meet the required exit-code and hash-verification condition.'
        }
    }
}

$GitExe = Resolve-Git
$FinalSha = Resolve-FinalSha $GitExe
Assert-CleanWorktree $GitExe
$Branch = Resolve-CurrentBranch $GitExe
$GhExe = Resolve-Gh
Assert-ReadyPullRequest $GhExe $Branch $FinalSha
$PythonExe = Resolve-PythonWithEsptool
$EsptoolWriteOperation = Resolve-EsptoolWriteOperation $PythonExe
if ([string]::IsNullOrWhiteSpace($Port)) { $Port = Resolve-DefaultPort }
$Port = $Port.Trim().ToUpperInvariant()
if (-not (Test-Port $Port)) {
    throw 'Port must be COM followed by digits, for example COM6.'
}
Resolve-ArtifactRuns $GhExe $FinalSha
$CiBuildSha = [string]$Items[0].BuildSha

Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
$state = Read-State $FinalSha $CiBuildSha
$script:CurrentIndex = $state.CurrentIndex
$script:ConfirmedIndexes = @($state.ConfirmedIndexes)
$script:CurrentFlashVerified = $false

$form = New-Object System.Windows.Forms.Form
$form.Text = 'ESP32-P4 CI Firmware Flasher'
$form.StartPosition = 'CenterScreen'
$form.ClientSize = New-Object System.Drawing.Size(900, 700)
$form.FormBorderStyle = 'FixedDialog'
$form.MaximizeBox = $false

function Add-Label([string]$Text, [int]$X, [int]$Y, [int]$Width = 860) {
    $label = New-Object System.Windows.Forms.Label
    $label.Text = $Text
    $label.Location = New-Object System.Drawing.Point($X, $Y)
    $label.Size = New-Object System.Drawing.Size($Width, 20)
    $form.Controls.Add($label)
    return $label
}

$repoLabel = Add-Label "Repository: $Repo" 15 15
$shaLabel = Add-Label "Final SHA: $FinalSha" 15 40
$boundaryLabel = Add-Label 'ESP32-P4 host firmware only; this tool does not update the ESP32-C6 hosted coprocessor.' 15 65
$portCaption = Add-Label 'Port:' 15 95 45
$portBox = New-Object System.Windows.Forms.TextBox
$portBox.Text = $state.Port
$portBox.Location = New-Object System.Drawing.Point(65, 92)
$portBox.Size = New-Object System.Drawing.Size(110, 22)
$form.Controls.Add($portBox)
$currentLabel = Add-Label '' 15 125
$statusLabel = Add-Label 'Status: Select Flash current to begin.' 15 150
$progressList = New-Object System.Windows.Forms.ListBox
$progressList.Font = New-Object System.Drawing.Font('Consolas', 9)
$progressList.Location = New-Object System.Drawing.Point(15, 180)
$progressList.Size = New-Object System.Drawing.Size(870, 250)
$form.Controls.Add($progressList)
$outputBox = New-Object System.Windows.Forms.TextBox
$outputBox.Multiline = $true
$outputBox.ReadOnly = $true
$outputBox.ScrollBars = 'Both'
$outputBox.WordWrap = $false
$outputBox.Font = New-Object System.Drawing.Font('Consolas', 9)
$outputBox.Location = New-Object System.Drawing.Point(15, 440)
$outputBox.Size = New-Object System.Drawing.Size(870, 200)
$form.Controls.Add($outputBox)
$flashButton = New-Object System.Windows.Forms.Button
$flashButton.Text = 'Flash current'
$flashButton.Location = New-Object System.Drawing.Point(15, 655)
$flashButton.Size = New-Object System.Drawing.Size(145, 32)
$form.Controls.Add($flashButton)
$confirmButton = New-Object System.Windows.Forms.Button
$confirmButton.Text = 'Mark PASS and flash next'
$confirmButton.Location = New-Object System.Drawing.Point(170, 655)
$confirmButton.Size = New-Object System.Drawing.Size(215, 32)
$confirmButton.Enabled = $false
$form.Controls.Add($confirmButton)
$exitButton = New-Object System.Windows.Forms.Button
$exitButton.Text = 'Exit'
$exitButton.Location = New-Object System.Drawing.Point(765, 655)
$exitButton.Size = New-Object System.Drawing.Size(120, 32)
$form.Controls.Add($exitButton)

function Update-CurrentDisplay {
    $item = $Items[$script:CurrentIndex - 1]
    $currentLabel.Text = "Current: $($item.Index)/$($Items.Count) Artifact: $($item.Artifact) Run: $($item.Run)"
    $progressList.Items.Clear()
    foreach ($progressItem in $Items) {
        $prefix = if ($script:ConfirmedIndexes -contains $progressItem.Index) {
            '[PASS]'
        } elseif ($progressItem.Index -eq $script:CurrentIndex) {
            '[CURRENT]'
        } else {
            '[WAIT]'
        }
        [void]$progressList.Items.Add(('{0} {1}: {2}' -f $prefix, $progressItem.Index, $progressItem.Artifact))
    }
    $progressList.SelectedIndex = $script:CurrentIndex - 1
}

function Set-Busy([bool]$Busy) {
    $complete = $script:CurrentIndex -eq $Items.Count -and $script:ConfirmedIndexes -contains $Items.Count
    $flashButton.Enabled = (-not $Busy) -and (-not $complete)
    $confirmButton.Enabled = (-not $Busy) -and $script:CurrentFlashVerified -and (-not $complete)
    $exitButton.Enabled = -not $Busy
    $portBox.Enabled = -not $Busy
    $form.UseWaitCursor = $Busy
    [System.Windows.Forms.Application]::DoEvents()
}

function Flash-CurrentItem {
    $selectedPort = $portBox.Text.Trim().ToUpperInvariant()
    if (-not (Test-Port $selectedPort)) {
        [System.Windows.Forms.MessageBox]::Show('Port must be COM followed by digits, for example COM6.', 'Invalid port') | Out-Null
        return
    }
    $script:CurrentFlashVerified = $false
    Set-Busy $true
    $item = $Items[$script:CurrentIndex - 1]
    $statusLabel.Text = "Status: Flashing item $($item.Index) on $selectedPort..."
    try {
        $result = Invoke-CurrentFlash $item $selectedPort $GhExe $PythonExe $EsptoolWriteOperation $FinalSha
        $outputBox.Text = "Log: $($result.LogPath)`r`n`r`n$($result.Output)"
        if ($result.Success) {
            Save-State $script:CurrentIndex $script:ConfirmedIndexes $selectedPort $FinalSha $CiBuildSha
            $statusLabel.Text = "Status: $($result.Detail) Check the device, then mark PASS to continue."
            $script:CurrentFlashVerified = $true
        } else {
            $statusLabel.Text = "Status: $($result.Detail) Current item was not advanced. Log: $($result.LogPath)"
        }
    }
    catch {
        $outputBox.Text = $_ | Out-String
        $statusLabel.Text = "Status: Error. Current item was not advanced. $($_.Exception.Message)"
    }
    finally { Set-Busy $false }
}

$flashButton.Add_Click({ Flash-CurrentItem })
$confirmButton.Add_Click({
    if (-not $script:CurrentFlashVerified) { return }
    $selectedPort = $portBox.Text.Trim().ToUpperInvariant()
    $next = Get-NextProgress $script:CurrentIndex $script:ConfirmedIndexes $Items.Count
    $script:CurrentIndex = $next.CurrentIndex
    $script:ConfirmedIndexes = @($next.ConfirmedIndexes)
    $script:CurrentFlashVerified = $false
    Save-State $script:CurrentIndex $script:ConfirmedIndexes $selectedPort $FinalSha $CiBuildSha
    Update-CurrentDisplay
    if ($next.Completed) {
        Set-Busy $false
        $statusLabel.Text = "Status: All $($Items.Count) items are confirmed."
        return
    }
    Flash-CurrentItem
})
$exitButton.Add_Click({ $form.Close() })
$progressList.Add_SelectedIndexChanged({
    if ($progressList.SelectedIndex -ne ($script:CurrentIndex - 1)) {
        $progressList.SelectedIndex = $script:CurrentIndex - 1
    }
})
Update-CurrentDisplay
if ($script:CurrentIndex -eq $Items.Count -and $script:ConfirmedIndexes -contains $Items.Count) {
    Set-Busy $false
    $statusLabel.Text = "Status: All $($Items.Count) items are confirmed."
}
[void]$form.ShowDialog()
