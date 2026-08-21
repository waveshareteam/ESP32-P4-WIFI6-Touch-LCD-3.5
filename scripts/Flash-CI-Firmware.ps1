[CmdletBinding()]
param(
    [string]$Port = '',
    [switch]$ListOnly,
    [switch]$SelfTest
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$Repo = 'waveshareteam/ESP32-P4-WIFI6-Touch-LCD-3.5'
$ExpectedTarget = 'esp32p4'
$ExpectedFramework = 'ESP-IDF'
$ExpectedBaud = 460800
$ArtifactPolicyCapacity = 32MB
$DeviceFlashCapacity = 16MB
$SupportedDeclaredFlashCapacities = @(2MB, 4MB, 8MB, 16MB)
$StateVersion = 2
$DefaultStartIndex = 1
$RevisionWarning = 'Silicon revision cannot establish PCB/electrical revision.'
$ProjectNames = @(
    '01_HowToCreateProject', '02_HelloWorld', '03_i2c_tools', '04_wifistation',
    '05_sdmmc', '06_I2SCodec', '07_Displaycolorbar', '08_lvgl_demo_v9',
    '09_video_lcd_display', '10_mp4_player', '11_esp_brookesia_phone',
    '12_esp32-p4-eye'
)
$Versions = @('v5.5.5', 'v6.0.2')
$Items = @()
$itemIndex = 1
foreach ($projectName in $ProjectNames) {
    foreach ($version in $Versions) {
        $prefix = "esp-idf-$version-$projectName-esp32p4-rev3_x-"
        $Items += [pscustomobject]@{
            Index = $itemIndex; Workflow = 'esp-idf.yml'; Profile = 'rev3_x'
            Name = $projectName; Framework = $ExpectedFramework; Version = $version
            SourceProject = "examples/esp-idf/$projectName"; Prefix = $prefix
            Artifact = "$prefix<final-sha>"; BuildSha = '<final-sha>'; Run = $null
        }
        $itemIndex++
    }
}
foreach ($profile in @('rev1_3', 'rev3_x')) {
    $prefix = "product-firmware-v6.0.2-esp32p4-$profile-"
    $Items += [pscustomobject]@{
        Index = $itemIndex; Workflow = 'product-firmware.yml'; Profile = $profile
        Name = '12_esp32-p4-eye'; Framework = $ExpectedFramework; Version = 'v6.0.2'
        SourceProject = 'examples/esp-idf/12_esp32-p4-eye'; Prefix = $prefix
        Artifact = "$prefix<final-sha>"; BuildSha = '<final-sha>'; Run = $null
    }
    $itemIndex++
}

function Test-Port([string]$Value) { return $Value -match '^COM\d+$' }

function Get-FileSha256([string]$Path) {
    $stream = $null; $algorithm = $null
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

function Test-RelativePackagePath([string]$PackageRoot, [string]$RelativePath) {
    if ([string]::IsNullOrWhiteSpace($RelativePath) -or
        $RelativePath.Contains('\') -or [System.IO.Path]::IsPathRooted($RelativePath) -or
        $RelativePath -notmatch '^[^/]+(?:/[^/]+)*$' -or
        @($RelativePath.Split('/') | Where-Object { $_ -in @('.', '..') }).Count) { return $false }
    $root = [System.IO.Path]::GetFullPath($PackageRoot).TrimEnd([System.IO.Path]::DirectorySeparatorChar, [System.IO.Path]::AltDirectorySeparatorChar) + [System.IO.Path]::DirectorySeparatorChar
    $candidate = [System.IO.Path]::GetFullPath((Join-Path $PackageRoot ($RelativePath.Replace('/', [System.IO.Path]::DirectorySeparatorChar))))
    return $candidate.StartsWith($root, [System.StringComparison]::OrdinalIgnoreCase)
}

function Get-ArtifactBuildSha([string]$ArtifactName, [string]$ExpectedPrefix) {
    $match = [regex]::Match($ArtifactName, ('^' + [regex]::Escape($ExpectedPrefix) + '([0-9a-fA-F]{40})$'))
    if (-not $match.Success) { throw "Artifact name does not match $ExpectedPrefix<final-sha>." }
    return $match.Groups[1].Value.ToLowerInvariant()
}

function Get-RevisionProfile([int]$Major, [int]$Minor) {
    if ($Major -lt 0 -or $Minor -lt 0) { throw 'Invalid silicon revision.' }
    if ($Major -eq 1) { return 'rev1_3' }
    if ($Major -eq 3) { return 'rev3_x' }
    throw "Unsupported ESP32-P4 silicon revision v$Major.$Minor; supported ranges are [1.0, 2.0) and [3.0, 4.0)."
}

function Parse-SiliconProbe([string]$ProbeText) {
    $match = [regex]::Match($ProbeText, '(?im)\bESP32[- ]P4\b[^\r\n]*?\brevision\s+v(?<major>\d+)\.(?<minor>\d+)\b')
    if (-not $match.Success) { throw "Refusing to flash: no explicit ESP32-P4 revision vMAJOR.MINOR. $RevisionWarning" }
    $major = [int]$match.Groups['major'].Value; $minor = [int]$match.Groups['minor'].Value
    return [pscustomobject]@{ Chip = 'ESP32-P4'; Revision = "v$major.$minor"; Profile = Get-RevisionProfile $major $minor }
}

function Assert-ProfileForSilicon($Item, $Silicon) {
    if ([string]$Item.Profile -ne [string]$Silicon.Profile) {
        throw "Refusing profile $($Item.Profile) for ESP32-P4 $($Silicon.Revision); detected profile is $($Silicon.Profile). $RevisionWarning"
    }
}

function Test-NoC6Content([string]$PackageDir, [string]$ManifestText) {
    $pattern = '(?i)(?:esp32[-_]?c6|\bc6\b)'
    if ($ManifestText -match $pattern) { throw 'Package manifest identifies ESP32-C6 content.' }
    $base = [System.IO.Path]::GetFullPath($PackageDir).TrimEnd([System.IO.Path]::DirectorySeparatorChar, [System.IO.Path]::AltDirectorySeparatorChar) + [System.IO.Path]::DirectorySeparatorChar
    foreach ($file in @(Get-ChildItem -LiteralPath $PackageDir -Recurse -File)) {
        $relative = $file.FullName.Substring($base.Length).Replace('\', '/')
        if ($relative -match $pattern) { throw "Package path identifies ESP32-C6 content: $relative" }
        if ($file.Length -le 1MB -and $file.Extension -in @('.json', '.txt', '.sh', '.bat', '.args') -and (Get-Content -LiteralPath $file.FullName -Raw) -match $pattern) {
            throw "Package text identifies ESP32-C6 content: $relative"
        }
    }
}

function Test-NoEraseContent([string]$PackageDir, [string]$ManifestText) {
    $pattern = '(?i)(?:--?erase[-_]?all|erase[-_]?(?:all|flash|region))'
    if ($ManifestText -match $pattern) { throw 'Package manifest contains a forbidden erase operation.' }
    foreach ($file in @(Get-ChildItem -LiteralPath $PackageDir -Recurse -File)) {
        if ($file.Length -le 1MB -and ($file.Extension -in @('.json', '.txt', '.sh', '.bat', '.args') -or $file.Name -eq 'flash_args') -and (Get-Content -LiteralPath $file.FullName -Raw) -match $pattern) {
            throw "Package helper contains a forbidden erase operation: $($file.Name)"
        }
    }
}

function Get-EspImageChipId([string]$Path) {
    $header = [System.IO.File]::ReadAllBytes($Path)
    if ($header.Length -eq 0 -or $header[0] -ne 0xE9) { return $null }
    if ($header.Length -lt 24) { throw "ESP image header is truncated: $Path" }
    if ($header[1] -lt 1 -or $header[1] -gt 16) { throw "ESP image header has an unsafe segment count: $Path" }
    $chipId = [BitConverter]::ToUInt16($header, 12)
    if ($chipId -ne 18) { throw "ESP image header is not ESP32-P4: $Path" }
    return [int]$chipId
}

function Get-EffectiveFlashCapacity($Manifest) {
    foreach ($property in @('artifact_policy_capacity_bytes', 'device_flash_capacity_bytes', 'declared_flash_size_bytes')) {
        if (-not $Manifest.PSObject.Properties[$property]) { throw 'Package manifest capacity contract is incomplete.' }
    }
    if (($Manifest.artifact_policy_capacity_bytes -isnot [long] -and $Manifest.artifact_policy_capacity_bytes -isnot [int]) -or
        ($Manifest.device_flash_capacity_bytes -isnot [long] -and $Manifest.device_flash_capacity_bytes -isnot [int]) -or
        [int64]$Manifest.artifact_policy_capacity_bytes -ne [int64]$ArtifactPolicyCapacity -or
        [int64]$Manifest.device_flash_capacity_bytes -ne [int64]$DeviceFlashCapacity) {
        throw 'Package manifest capacity contract is unsafe.'
    }
    $declared = $Manifest.declared_flash_size_bytes
    if ($null -ne $declared) {
        if ($declared -isnot [long] -and $declared -isnot [int]) { throw 'Package manifest declared flash size is unsafe.' }
        $declaredBytes = [int64]$declared
        if ($SupportedDeclaredFlashCapacities -notcontains $declaredBytes) { throw 'Package manifest declared flash size is unsupported.' }
        return [Math]::Min([Math]::Min([int64]$ArtifactPolicyCapacity, [int64]$DeviceFlashCapacity), $declaredBytes)
    }
    return [Math]::Min([int64]$ArtifactPolicyCapacity, [int64]$DeviceFlashCapacity)
}

function Test-PackageManifest([string]$PackageDir, $Item, [string]$ArtifactSha) {
    $manifestPath = Join-Path $PackageDir 'manifest.json'
    if (-not (Test-Path -LiteralPath $manifestPath -PathType Leaf)) { throw 'Package manifest.json is missing.' }
    $manifestText = Get-Content -LiteralPath $manifestPath -Raw
    $manifest = $manifestText | ConvertFrom-Json
    if ([int]$manifest.schema_version -ne 2 -or [string]$manifest.artifact_kind -ne 'esp-idf-flashable' -or
        [string]$manifest.profile -ne $Item.Profile -or [string]$manifest.name -ne $Item.Name -or
        [string]$manifest.framework -ine $Item.Framework -or [string]$manifest.framework_version -ne $Item.Version -or
        [string]$manifest.target -ne $ExpectedTarget -or [string]$manifest.project_path -ne $Item.SourceProject -or
        [string]$manifest.git_sha -ne $ArtifactSha -or $manifest.host_only -isnot [bool] -or -not [bool]$manifest.host_only -or
        $manifest.contains_c6_firmware -isnot [bool] -or [bool]$manifest.contains_c6_firmware) {
        throw 'Package manifest identity, profile, or host-only boundary is unsafe.'
    }
    if ([int64]$manifest.baud -ne $ExpectedBaud -or @($manifest.files).Count -lt 1 -or
        [string]$manifest.flash_command -notmatch '(?i)\bwrite(?:-|_)flash\b' -or
        [string]$manifest.flash_command -match '(?i)(?:--?erase[-_]?all|erase[-_]?(?:all|flash|region))') {
        throw 'Package manifest flash metadata is incomplete or unsafe.'
    }
    $effectiveFlashCapacity = Get-EffectiveFlashCapacity $manifest
    Test-NoC6Content $PackageDir $manifestText
    Test-NoEraseContent $PackageDir $manifestText
    $plan = @(); $offsets = @{}; $p4ImageCount = 0
    foreach ($file in @($manifest.files)) {
        $relativePath = [string]$file.path
        if (-not (Test-RelativePackagePath $PackageDir $relativePath) -or [string]$file.sha256 -notmatch '^[0-9a-fA-F]{64}$' -or
            [int64]$file.size -le 0 -or [string]$file.offset -notmatch '^0x[0-9a-fA-F]+$') { throw "Manifest file metadata is unsafe: $relativePath" }
        $fullPath = [System.IO.Path]::GetFullPath((Join-Path $PackageDir ($relativePath.Replace('/', [System.IO.Path]::DirectorySeparatorChar))))
        if (-not (Test-Path -LiteralPath $fullPath -PathType Leaf)) { throw "Manifest file is missing: $relativePath" }
        $actualHash = Get-FileSha256 $fullPath; $actualSize = [int64](Get-Item -LiteralPath $fullPath).Length
        if ($actualHash -ne ([string]$file.sha256).ToLowerInvariant() -or $actualSize -ne [int64]$file.size) { throw "Manifest checksum or size verification failed: $relativePath" }
        $actualChipId = Get-EspImageChipId $fullPath
        if ($null -eq $actualChipId) {
            if ($null -ne $file.image_chip_id) { throw "Raw manifest file has an image_chip_id marker: $relativePath" }
        }
        elseif ($actualChipId -ne 18 -or [int]$file.image_chip_id -ne $actualChipId) { throw "Manifest image_chip_id does not match the ESP32-P4 header: $relativePath" }
        else { $p4ImageCount++ }
        $offset = [Convert]::ToInt64(([string]$file.offset).Substring(2), 16)
        if ($offsets.ContainsKey($offset) -or $offset + $actualSize -gt $effectiveFlashCapacity) { throw "Manifest flash range is unsafe: $relativePath" }
        $offsets[$offset] = $true; $plan += [pscustomobject]@{ Offset = $offset; Size = $actualSize; Path = $fullPath }
    }
    $orderedPlan = @($plan | Sort-Object Offset)
    for ($index = 1; $index -lt $orderedPlan.Count; ++$index) {
        if ($orderedPlan[$index - 1].Offset + $orderedPlan[$index - 1].Size -gt $orderedPlan[$index].Offset) { throw 'Package manifest contains overlapping flash ranges.' }
    }
    if ($p4ImageCount -lt 1) { throw 'Package manifest has no valid ESP32-P4 image header.' }
    return $orderedPlan
}

function Get-StateForBuild($Saved, [string]$FinalSha, [string]$BuildSha, [string]$Profile, [string]$DefaultPort, [int]$ItemCount) {
    if (-not $Saved -or -not $Saved.PSObject.Properties['StateVersion'] -or -not $Saved.PSObject.Properties['FinalSha'] -or
        -not $Saved.PSObject.Properties['BuildSha'] -or -not $Saved.PSObject.Properties['Profile'] -or
        -not $Saved.PSObject.Properties['CurrentIndex'] -or -not $Saved.PSObject.Properties['ConfirmedIndexes'] -or
        [int]$Saved.StateVersion -ne $StateVersion -or [string]$Saved.FinalSha -ne $FinalSha -or
        [string]$Saved.BuildSha -ne $BuildSha -or [string]$Saved.Profile -ne $Profile -or
        -not $Saved.PSObject.Properties['Port'] -or [string]$Saved.Port -ne $DefaultPort) {
        return [pscustomobject]@{ StateVersion = $StateVersion; FinalSha = $FinalSha; BuildSha = $BuildSha; Profile = $Profile; Port = $DefaultPort; CurrentIndex = $DefaultStartIndex; ConfirmedIndexes = @(); Completed = $false; Attempts = @(); Results = @() }
    }
    $confirmed = @($Saved.ConfirmedIndexes | ForEach-Object { [int]$_ } | Where-Object { $_ -ge 1 -and $_ -le $ItemCount } | Sort-Object -Unique)
    # State files written before Completed existed remain compatible: full confirmation is completion.
    $completed = ($confirmed.Count -eq $ItemCount) -or ($Saved.PSObject.Properties['Completed'] -and [bool]$Saved.Completed)
    if ($completed) {
        $index = -1
    }
    else {
        $index = [int]$Saved.CurrentIndex
        if ($index -lt 1 -or $index -gt $ItemCount) { throw "Saved CurrentIndex is outside 1..$ItemCount." }
    }
    $attempts = if ($Saved.PSObject.Properties['Attempts']) { @($Saved.Attempts) } else { @() }
    $results = if ($Saved.PSObject.Properties['Results']) { @($Saved.Results) } else { @() }
    return [pscustomobject]@{ StateVersion = $StateVersion; FinalSha = $FinalSha; BuildSha = $BuildSha; Profile = $Profile; Port = $DefaultPort; CurrentIndex = $index; ConfirmedIndexes = $confirmed; Completed = $completed; Attempts = $attempts; Results = $results }
}

function Get-NextProgress([int]$CurrentIndex, [int[]]$ConfirmedIndexes, [int]$ItemCount) {
    if ($CurrentIndex -lt 1 -or $CurrentIndex -gt $ItemCount) { throw 'Progress indexes must be within the item range.' }
    return [pscustomobject]@{ CurrentIndex = if ($CurrentIndex -eq $ItemCount) { $CurrentIndex } else { $CurrentIndex + 1 }; ConfirmedIndexes = @($ConfirmedIndexes + $CurrentIndex | Where-Object { $_ -ge 1 -and $_ -le $ItemCount } | Sort-Object -Unique); Completed = $CurrentIndex -eq $ItemCount }
}

function Save-State($State, [string]$Path) {
    [ordered]@{
        StateVersion = $State.StateVersion; FinalSha = $State.FinalSha; BuildSha = $State.BuildSha
        Profile = $State.Profile; Port = $State.Port; CurrentIndex = $State.CurrentIndex
        ConfirmedIndexes = @($State.ConfirmedIndexes); Completed = [bool]$State.Completed
        Attempts = @($State.Attempts); Results = @($State.Results)
    } | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $Path -Encoding UTF8
}

function New-ProgressRecord($Item, $State, [string]$Status, [string]$LogPath) {
    return [pscustomobject][ordered]@{
        Index = $Item.Index; Item = "$($Item.Workflow):$($Item.Profile):$($Item.Name):$($Item.Version)"
        SourceProject = $Item.SourceProject; Artifact = $Item.Artifact; FinalSha = $State.FinalSha; BuildSha = $State.BuildSha
        Profile = $State.Profile; Port = $State.Port; Status = $Status
        TimestampUtc = [DateTime]::UtcNow.ToString('o'); LogPath = $LogPath
    }
}

function Resolve-Executable([string]$Name, [string[]]$Fallbacks) {
    $command = Get-Command $Name -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($command -and $command.Source) { return $command.Source }
    foreach ($candidate in $Fallbacks) { if (Test-Path -LiteralPath $candidate -PathType Leaf) { return $candidate } }
    throw "$Name was not found on PATH or in the supported fallback locations."
}
function Resolve-Git { return Resolve-Executable 'git' @((Join-Path ${env:ProgramFiles} 'Git\cmd\git.exe'), (Join-Path ${env:ProgramFiles} 'Git\bin\git.exe'), 'C:\Git\cmd\git.exe', 'D:\Git\cmd\git.exe') }
function Resolve-Gh { return Resolve-Executable 'gh' @((Join-Path ${env:ProgramFiles} 'GitHub CLI\gh.exe'), (Join-Path ${env:ProgramFiles} 'GitHub CLI\bin\gh.exe')) }
function Resolve-PythonWithEsptool {
    $candidates = @(); $command = Get-Command python -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($command -and $command.Source) { $candidates += $command.Source }
    foreach ($root in @((Join-Path $env:USERPROFILE '.espressif\python_env'), 'C:\Espressif', 'D:\espressif')) {
        if (Test-Path -LiteralPath $root) { $candidates += @(Get-ChildItem -LiteralPath $root -Recurse -File -Filter python.exe -ErrorAction SilentlyContinue | Where-Object { $_.FullName -match '[\\/]python_env[\\/].+[\\/]Scripts[\\/]python\.exe$' } | ForEach-Object FullName) }
    }
    foreach ($candidate in @($candidates | Select-Object -Unique)) { & $candidate -c 'import esptool' *> $null; if ($LASTEXITCODE -eq 0) { return $candidate } }
    throw 'No Python interpreter with esptool was found.'
}
function Resolve-EsptoolWriteOperation([string]$PythonExe) {
    foreach ($operation in @('write-flash', 'write_flash')) { & $PythonExe -m esptool $operation --help *> $null; if ($LASTEXITCODE -eq 0) { return $operation } }
    throw 'The installed esptool does not expose a supported write-flash operation.'
}
function Resolve-DefaultPort {
    $ports = @(Get-CimInstance Win32_PnPEntity -ErrorAction SilentlyContinue | Where-Object { $_.PNPDeviceID -match 'VID_303A&PID_1001' -and $_.Name -match '\(COM\d+\)' } | ForEach-Object { [regex]::Match($_.Name, '\((COM\d+)\)').Groups[1].Value } | Sort-Object -Unique)
    if ($ports.Count -eq 1) { return $ports[0] }; throw 'Unable to identify exactly one ESP32-P4 USB serial port; pass -Port COMx.'
}
function Probe-Silicon([string]$PythonExe, [string]$ProbePort) {
    foreach ($operation in @('flash-id', 'flash_id')) { $output = (& $PythonExe -m esptool --chip $ExpectedTarget --port $ProbePort $operation 2>&1 | Out-String); try { return Parse-SiliconProbe $output } catch {} }
    throw "Refusing to flash ${ProbePort}: esptool did not establish an ESP32-P4 revision. $RevisionWarning"
}

function Assert-LocalAndPullRequest([string]$GitExe, [string]$GhExe, [string]$RepoRoot) {
    $finalSha = (& $GitExe -C $RepoRoot rev-parse HEAD 2>&1 | Out-String).Trim().ToLowerInvariant()
    if ($LASTEXITCODE -ne 0 -or $finalSha -notmatch '^[0-9a-f]{40}$') { throw 'Unable to resolve a full local git HEAD SHA.' }
    if (-not [string]::IsNullOrWhiteSpace((& $GitExe -C $RepoRoot status --porcelain=v1 --untracked-files=all 2>&1 | Out-String))) { throw 'Refusing to continue: the working tree must be clean.' }
    $branch = (& $GitExe -C $RepoRoot symbolic-ref --quiet --short HEAD 2>&1 | Out-String).Trim()
    if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($branch)) { throw 'Refusing to continue: check out a non-detached branch first.' }
    $prs = @((& $GhExe pr list --repo $Repo --head $branch --state open --limit 2 --json state,isDraft,headRefName,headRefOid 2>&1 | ConvertFrom-Json))
    if ($LASTEXITCODE -ne 0 -or $prs.Count -ne 1 -or [bool]$prs[0].isDraft -or [string]$prs[0].state -ine 'OPEN' -or [string]$prs[0].headRefName -ne $branch -or [string]$prs[0].headRefOid -ine $finalSha) { throw 'Refusing to continue: one open, ready, exact-head pull request is required.' }
    return $finalSha
}
function Resolve-ArtifactRuns($SelectedItems, [string]$GhExe, [string]$FinalSha) {
    foreach ($group in @($SelectedItems | Group-Object Workflow)) {
        $raw = (& $GhExe run list --repo $Repo --workflow $group.Name --commit $FinalSha --limit 20 --json databaseId,headSha,createdAt,status,conclusion 2>&1 | Out-String)
        if ($LASTEXITCODE -ne 0) { throw "Unable to list $($group.Name) runs: $raw" }
        $runs = @($raw | ConvertFrom-Json | Where-Object { [string]$_.headSha -ieq $FinalSha } | Sort-Object createdAt -Descending)
        if ($runs.Count -lt 1 -or [string]$runs[0].status -ine 'completed' -or [string]$runs[0].conclusion -ine 'success') { throw "Latest $($group.Name) exact-final-SHA run is not successful." }
        $run = [string]$runs[0].databaseId
        $artifacts = @(((& $GhExe api "repos/$Repo/actions/runs/$run/artifacts?per_page=100" 2>&1 | ConvertFrom-Json).artifacts) | Where-Object { -not [bool]$_.expired })
        foreach ($item in $group.Group) {
            $matches = @($artifacts | Where-Object { [string]$_.name -match ('^' + [regex]::Escape($item.Prefix) + [regex]::Escape($FinalSha) + '$') })
            if ($matches.Count -ne 1) { throw "Expected one non-expired exact profile-qualified artifact $($item.Prefix)$FinalSha." }
            $item.Artifact = [string]$matches[0].name; $item.BuildSha = Get-ArtifactBuildSha $item.Artifact $item.Prefix; $item.Run = $run
            if ($item.BuildSha -ne $FinalSha) { throw 'Artifact SHA is not the final HEAD SHA.' }
        }
    }
}
function Test-HashVerificationOutput([string]$Output, [int]$ExpectedSegments, [int]$ExitCode) {
    if ($ExitCode -ne 0) { throw "Flash command exited with $ExitCode." }
    $verified = @([regex]::Matches($Output, '(?im)^Hash of data verified\.?\s*$'))
    if ($verified.Count -lt $ExpectedSegments) { throw "esptool reported $($verified.Count) hash-verification lines for $ExpectedSegments planned flash segments." }
}

function Ensure-StateRoot {
    if (-not (Test-Path -LiteralPath $StateRoot)) {
        New-Item -ItemType Directory -Path $StateRoot | Out-Null
    }
}

function New-RunPaths {
    Ensure-StateRoot
    $stamp = Get-Date -Format 'yyyyMMdd-HHmmss-fff'
    $downloadRoot = Join-Path $StateRoot 'downloads'
    $logRoot = Join-Path $StateRoot 'logs'
    foreach ($directory in @($downloadRoot, $logRoot)) {
        if (-not (Test-Path -LiteralPath $directory)) {
            New-Item -ItemType Directory -Path $directory | Out-Null
        }
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

function Invoke-FlashItem($Item, $State, [string]$StateFile, [string]$FlashPort, [string]$PythonExe, [string]$GhExe, [string]$WriteOperation) {
    $paths = New-RunPaths
    $attempt = New-ProgressRecord $Item $State 'started' $paths.LogPath
    $State.Attempts = @($State.Attempts) + $attempt
    Save-State $State $StateFile
    Add-RunLog $paths.LogPath "buildSHA=$($Item.BuildSha) profile=$($Item.Profile) artifact=$($Item.Artifact) run=$($Item.Run) port=$FlashPort"
    try {
        $silicon = Probe-Silicon $PythonExe $FlashPort
        Assert-ProfileForSilicon $Item $silicon
        Add-RunLog $paths.LogPath "silicon=$($silicon.Revision)"
        $downloadOutput = (& $GhExe run download $Item.Run --repo $Repo --name $Item.Artifact --dir $paths.DownloadDir 2>&1 | Out-String)
        $downloadExitCode = $LASTEXITCODE
        Add-RunLog $paths.LogPath $downloadOutput
        if ($downloadExitCode -ne 0) { throw "Artifact download failed with exit code $downloadExitCode." }
        foreach ($archive in @(Get-ChildItem -LiteralPath $paths.DownloadDir -Recurse -Filter '*.zip')) {
            $destination = Join-Path $archive.DirectoryName ($archive.BaseName + '-unzipped')
            if (Test-Path -LiteralPath $destination) { throw "Refusing to overwrite extraction directory: $destination" }
            Expand-Archive -LiteralPath $archive.FullName -DestinationPath $destination
        }
        $manifests = @(Get-ChildItem -LiteralPath $paths.DownloadDir -Recurse -Filter manifest.json)
        if ($manifests.Count -ne 1) { throw 'Expected exactly one package manifest.' }
        $plan = @(Test-PackageManifest $manifests[0].DirectoryName $Item $Item.BuildSha)
        $arguments = @('-m', 'esptool', '--port', $FlashPort, '--chip', $ExpectedTarget, '--baud', $ExpectedBaud, $WriteOperation)
        foreach ($segment in $plan) { $arguments += ('0x{0:X}' -f $segment.Offset); $arguments += $segment.Path }
        $output = (& $PythonExe @arguments 2>&1 | Out-String)
        $flashExitCode = $LASTEXITCODE
        Add-RunLog $paths.LogPath $output
        Test-HashVerificationOutput $output $plan.Count $flashExitCode
        $attempt.Status = 'success'; $attempt.TimestampUtc = [DateTime]::UtcNow.ToString('o')
        Save-State $State $StateFile
        return [pscustomobject]@{ Output = $output; LogPath = $paths.LogPath }
    }
    catch {
        Add-RunLog $paths.LogPath ($_ | Out-String)
        $attempt.Status = 'failed'; $attempt.TimestampUtc = [DateTime]::UtcNow.ToString('o')
        Save-State $State $StateFile
        throw "$($_.Exception.Message) Log: $($paths.LogPath)"
    }
}

function Invoke-SelfTest {
    if ($Items.Count -ne 26 -or @($Items | Where-Object Profile -eq 'rev1_3').Count -ne 1 -or @($Items | Where-Object Profile -eq 'rev3_x').Count -ne 25) { throw 'SelfTest expected one rev1_3 and 25 rev3_x items.' }
    if ((Parse-SiliconProbe 'Chip is ESP32-P4 (revision v1.3)').Profile -ne 'rev1_3' -or (Parse-SiliconProbe 'Chip is ESP32-P4 (revision v3.0)').Profile -ne 'rev3_x') { throw 'SelfTest silicon profile routing failed.' }
    $unsupportedRevisions = @(
        [pscustomobject]@{ Major = 0; Minor = 9 }
        [pscustomobject]@{ Major = 2; Minor = 0 }
        [pscustomobject]@{ Major = 4; Minor = 0 }
    )
    foreach ($unsupportedRevision in $unsupportedRevisions) {
        $rejected = $false
        try { [void](Get-RevisionProfile $unsupportedRevision.Major $unsupportedRevision.Minor) } catch { $rejected = $true }
        if (-not $rejected) { throw "SelfTest did not reject unsupported revision v$($unsupportedRevision.Major).$($unsupportedRevision.Minor)." }
    }
    $sha = '0123456789abcdef0123456789abcdef01234567'; if ((Get-ArtifactBuildSha ($Items[0].Prefix + $sha) $Items[0].Prefix) -ne $sha) { throw 'SelfTest artifact SHA binding failed.' }
    $reset = Get-StateForBuild ([pscustomobject]@{ StateVersion = 1; FinalSha = $sha; BuildSha = $sha; Profile = 'rev1_3' }) $sha $sha 'rev1_3' 'COM17' 25
    if ($reset.CurrentIndex -ne 1) { throw 'SelfTest state version reset failed.' }
    $root = Join-Path ([System.IO.Path]::GetTempPath()) ('waveshare-flasher-' + [guid]::NewGuid().ToString('N'))
    try {
        New-Item -ItemType Directory -Path $root | Out-Null
        $saved = [pscustomobject]@{ StateVersion = $StateVersion; FinalSha = $sha; BuildSha = $sha; Profile = 'rev1_3'; Port = 'COM17'; CurrentIndex = 3; ConfirmedIndexes = @(1, 2) }
        $samePort = Get-StateForBuild $saved $sha $sha 'rev1_3' 'COM17' 25
        if ($samePort.CurrentIndex -ne 3 -or @($samePort.ConfirmedIndexes).Count -ne 2) { throw 'SelfTest same-port state recovery failed.' }
        $otherPort = Get-StateForBuild $saved $sha $sha 'rev1_3' 'COM18' 25
        $missingPort = Get-StateForBuild ([pscustomobject]@{ StateVersion = $StateVersion; FinalSha = $sha; BuildSha = $sha; Profile = 'rev1_3'; CurrentIndex = 3; ConfirmedIndexes = @(1, 2) }) $sha $sha 'rev1_3' 'COM17' 25
        if ($otherPort.CurrentIndex -ne 1 -or @($otherPort.ConfirmedIndexes).Count -ne 0 -or $missingPort.CurrentIndex -ne 1) { throw 'SelfTest port-bound state reset failed.' }
        $lastTransition = Get-NextProgress 25 @(1..24) 25
        if (-not $lastTransition.Completed -or $lastTransition.CurrentIndex -ne 25 -or @($lastTransition.ConfirmedIndexes).Count -ne 25) { throw 'SelfTest final transition failed.' }
        $legacyComplete = Get-StateForBuild ([pscustomobject]@{ StateVersion = $StateVersion; FinalSha = $sha; BuildSha = $sha; Profile = 'rev1_3'; Port = 'COM17'; CurrentIndex = 25; ConfirmedIndexes = @(1..25) }) $sha $sha 'rev1_3' 'COM17' 25
        if (-not $legacyComplete.Completed -or $legacyComplete.CurrentIndex -ne -1) { throw 'SelfTest legacy completed-state derivation failed.' }
        $roundTrip = Get-StateForBuild $saved $sha $sha 'rev1_3' 'COM17' 25
        $roundTrip.Attempts = @(New-ProgressRecord $Items[0] $roundTrip 'success' 'logs/test.log')
        $roundTrip.Results = @(New-ProgressRecord $Items[0] $roundTrip 'PASS' 'logs/test.log')
        $statePath = Join-Path $root 'state.json'; Save-State $roundTrip $statePath
        $persisted = Get-Content -LiteralPath $statePath -Raw | ConvertFrom-Json
        if ([bool]$persisted.Completed -ne $false -or @($persisted.Attempts).Count -ne 1 -or @($persisted.Results).Count -ne 1 -or [string]$persisted.Attempts[0].LogPath -ne 'logs/test.log') { throw 'SelfTest state JSON round-trip failed.' }
        $bin = Join-Path $root 'bin'; New-Item -ItemType Directory -Path $bin | Out-Null
        $one = Join-Path $bin 'one.bin'; $two = Join-Path $bin 'two.bin'; $p4Header = New-Object byte[] 24; $p4Header[0] = 0xE9; $p4Header[1] = 1; $p4Header[12] = 18; [System.IO.File]::WriteAllBytes($one, $p4Header); [System.IO.File]::WriteAllBytes($two, [byte[]](5, 6, 7, 8))
        $item = $Items[0]; $manifest = [ordered]@{ schema_version = 2; artifact_kind = 'esp-idf-flashable'; profile = $item.Profile; host_only = $true; contains_c6_firmware = $false; name = $item.Name; framework = $item.Framework; framework_version = $item.Version; target = $ExpectedTarget; project_path = $item.SourceProject; git_sha = $sha; baud = $ExpectedBaud; artifact_policy_capacity_bytes = [int64]$ArtifactPolicyCapacity; device_flash_capacity_bytes = [int64]$DeviceFlashCapacity; declared_flash_size_bytes = [int64](2MB); files = @([ordered]@{ offset = '0x2000'; path = 'bin/one.bin'; size = 24; sha256 = Get-FileSha256 $one; image_chip_id = 18 }, [ordered]@{ offset = '0x10000'; path = 'bin/two.bin'; size = 4; sha256 = Get-FileSha256 $two; image_chip_id = $null }); flash_command = 'esptool write_flash' }
        $manifestPath = Join-Path $root 'manifest.json'; $manifest | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $manifestPath -Encoding UTF8
        if (@(Test-PackageManifest $root $item $sha).Count -ne 2) { throw 'SelfTest did not hash-verify every planned segment.' }
        $manifest.files[0].offset = '0x200000'; $manifest | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $manifestPath -Encoding UTF8
        $rejected = $false; try { [void](Test-PackageManifest $root $item $sha) } catch { $rejected = $true }; if (-not $rejected) { throw 'SelfTest did not enforce the declared 2 MiB flash size.' }
        $manifest.files[0].offset = ('0x{0:x}' -f ($DeviceFlashCapacity - 24)); $manifest.declared_flash_size_bytes = [int64](16MB); $manifest | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $manifestPath -Encoding UTF8
        if (@(Test-PackageManifest $root $item $sha).Count -ne 2) { throw 'SelfTest did not accept the exact 16 MiB boundary.' }
        $manifest.files[0].offset = ('0x{0:x}' -f ($DeviceFlashCapacity - 23)); $manifest | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $manifestPath -Encoding UTF8
        $rejected = $false; try { [void](Test-PackageManifest $root $item $sha) } catch { $rejected = $true }; if (-not $rejected) { throw 'SelfTest accepted a range past 16 MiB.' }
        $manifest.files[0].offset = ('0x{0:x}' -f ($DeviceFlashCapacity - 24)); $manifest.declared_flash_size_bytes = $null; $manifest | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $manifestPath -Encoding UTF8
        if (@(Test-PackageManifest $root $item $sha).Count -ne 2) { throw 'SelfTest did not apply the physical cap without a declaration.' }
        $manifest.files[0].offset = '0x2000'; $manifest.artifact_policy_capacity_bytes = [int64](16MB); $manifest | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $manifestPath -Encoding UTF8
        $rejected = $false; try { [void](Test-PackageManifest $root $item $sha) } catch { $rejected = $true }; if (-not $rejected) { throw 'SelfTest accepted a changed artifact policy capacity.' }
        Test-HashVerificationOutput "Hash of data verified.`nHash of data verified." 2 0
        $insufficientHashOutput = $false; try { Test-HashVerificationOutput 'Hash of data verified.' 2 0 } catch { $insufficientHashOutput = $true }; if (-not $insufficientHashOutput) { throw 'SelfTest accepted incomplete hash-verification output.' }
        $manifest.flash_command = 'esptool --erase-all write_flash'; $manifest | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $manifestPath -Encoding UTF8
        $rejected = $false; try { [void](Test-PackageManifest $root $item $sha) } catch { $rejected = $true }; if (-not $rejected) { throw 'SelfTest did not reject --erase-all.' }
    }
    finally { if (Test-Path -LiteralPath $root) { Remove-Item -LiteralPath $root -Recurse -Force } }
    Write-Output 'SELF_TEST_OK items=26 rev1_3=1 rev3_x=25 unsupportedRevisionsRejected=0.9,2.0,4.0 hashVerifiedSegments=2 eraseAllRejected=true capacityContract=true stateVersionReset=true portBoundRecovery=true completedTransition=true jsonRoundTrip=true'
}

if ($SelfTest) { Invoke-SelfTest; return }
if ($ListOnly) {
    Write-Output 'finalSHA=resolved-at-runtime'; Write-Output 'port=probed-and-locked-at-runtime'
    foreach ($item in $Items) { Write-Output ('{0}: workflow={1} profile={2} artifact={3} project={4}' -f $item.Index, $item.Workflow, $item.Profile, $item.Artifact, $item.SourceProject) }
    return
}

$RepoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$StateRoot = Join-Path $env:LOCALAPPDATA 'Waveshare\ESP32-P4-WIFI6-Touch-LCD-3.5\ci-firmware'
$StatePath = Join-Path $StateRoot 'state-v2.json'
$git = Resolve-Git; $gh = Resolve-Gh; $finalSha = Assert-LocalAndPullRequest $git $gh $RepoRoot
$python = Resolve-PythonWithEsptool; $writeOperation = Resolve-EsptoolWriteOperation $python
if ([string]::IsNullOrWhiteSpace($Port)) { $Port = Resolve-DefaultPort }; $Port = $Port.Trim().ToUpperInvariant()
if (-not (Test-Port $Port)) { throw 'Port must be COM followed by digits.' }
$silicon = Probe-Silicon $python $Port
$selectedItems = @($Items | Where-Object { $_.Profile -eq $silicon.Profile })
Resolve-ArtifactRuns $selectedItems $gh $finalSha
$buildSha = @($selectedItems.BuildSha | Sort-Object -Unique); if ($buildSha.Count -ne 1 -or $buildSha[0] -ne $finalSha) { throw 'All selected artifacts must use the final HEAD SHA.' }
if (-not (Test-Path -LiteralPath $StateRoot)) { New-Item -ItemType Directory -Path $StateRoot | Out-Null }
$saved = if (Test-Path -LiteralPath $StatePath) { Get-Content -LiteralPath $StatePath -Raw | ConvertFrom-Json } else { $null }
$state = Get-StateForBuild $saved $finalSha $buildSha[0] $silicon.Profile $Port $selectedItems.Count

Add-Type -AssemblyName System.Windows.Forms; Add-Type -AssemblyName System.Drawing
$form = New-Object System.Windows.Forms.Form; $form.Text = "ESP32-P4 CI Firmware Flasher ($($silicon.Revision), $($silicon.Profile), $Port)"; $form.ClientSize = New-Object System.Drawing.Size(840, 480)
$list = New-Object System.Windows.Forms.ListBox; $list.Location = New-Object System.Drawing.Point(10, 10); $list.Size = New-Object System.Drawing.Size(820, 330)
$flashButton = New-Object System.Windows.Forms.Button; $flashButton.Text = 'Flash current'; $flashButton.Location = New-Object System.Drawing.Point(10, 375)
$passButton = New-Object System.Windows.Forms.Button; $passButton.Text = 'Mark PASS and flash next'; $passButton.Location = New-Object System.Drawing.Point(150, 375); $passButton.Enabled = $false
$status = New-Object System.Windows.Forms.Label; $status.Location = New-Object System.Drawing.Point(10, 425); $status.Size = New-Object System.Drawing.Size(820, 25)
$form.Controls.AddRange(@($list, $flashButton, $passButton, $status)); $script:currentIndex = $state.CurrentIndex; $script:lastFlashSucceeded = $false; $script:lastSuccessfulLogPath = ''
function Update-View {
    $list.Items.Clear(); for ($i = 0; $i -lt $selectedItems.Count; $i++) { $marker = if (-not $state.Completed -and $i + 1 -eq $script:currentIndex) { '[CURRENT]' } elseif ($state.ConfirmedIndexes -contains ($i + 1)) { '[PASS]' } else { '[WAIT]' }; [void]$list.Items.Add(('{0} {1}: {2}' -f $marker, $i + 1, $selectedItems[$i].Artifact)) }
    if (-not $state.Completed -and $script:currentIndex -ge 1 -and $script:currentIndex -le $selectedItems.Count) { $list.SelectedIndex = $script:currentIndex - 1 } else { $list.SelectedIndex = -1 }
}
$flashButton.Add_Click({
    if ($state.Completed -or $script:currentIndex -lt 1 -or $script:currentIndex -gt $selectedItems.Count) { return }
    $flashButton.Enabled = $false; $passButton.Enabled = $false; $form.UseWaitCursor = $true; $status.Text = 'Flashing after a fresh silicon probe...'; $form.Refresh()
    try { $result = Invoke-FlashItem $selectedItems[$script:currentIndex - 1] $state $StatePath $Port $python $gh $writeOperation; $script:lastFlashSucceeded = $true; $script:lastSuccessfulLogPath = $result.LogPath; $passButton.Enabled = $true; $status.Text = "Flash completed. Confirm board behavior, then select Mark PASS. Log: $($result.LogPath)"; [System.Windows.Forms.MessageBox]::Show("Log: $($result.LogPath)`r`n`r`n$($result.Output)", 'Flash output') | Out-Null }
    catch { $script:lastFlashSucceeded = $false; $status.Text = 'Flash failed; no progress was recorded.'; [System.Windows.Forms.MessageBox]::Show($_.Exception.Message, 'Flash failed') | Out-Null }
    finally { $flashButton.Enabled = -not $state.Completed -and $script:currentIndex -ge 1 -and $script:currentIndex -le $selectedItems.Count; $form.UseWaitCursor = $false }
})
$passButton.Add_Click({
    if ($state.Completed -or -not $script:lastFlashSucceeded -or $script:currentIndex -lt 1 -or $script:currentIndex -gt $selectedItems.Count) { return }
    $next = Get-NextProgress $script:currentIndex $state.ConfirmedIndexes $selectedItems.Count; $state.CurrentIndex = $next.CurrentIndex; $state.ConfirmedIndexes = $next.ConfirmedIndexes
    $state.Completed = $next.Completed
    if ($state.Completed) { $state.CurrentIndex = -1 }
    $state.Results = @($state.Results) + (New-ProgressRecord $selectedItems[$script:currentIndex - 1] $state 'PASS' $script:lastSuccessfulLogPath)
    Save-State $state $StatePath
    $script:currentIndex = $state.CurrentIndex; $script:lastFlashSucceeded = $false; $passButton.Enabled = $false
    if ($next.Completed) { $flashButton.Enabled = $false; $status.Text = 'All profile-qualified items were marked PASS.'; Update-View } else { $status.Text = 'Progress saved. Flash the next item when ready.'; Update-View }
})
Update-View
if ($state.Completed) { $flashButton.Enabled = $false; $passButton.Enabled = $false; $status.Text = 'All profile-qualified items were already marked PASS.' }
[void]$form.ShowDialog()
