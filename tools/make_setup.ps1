# Fabrique dist\Horloge-setup.exe : télécommande pré-configurée (identifiant + clé) à distribuer.
#
# Utilisation (dans un terminal, depuis le dossier du projet) :
#   powershell -ExecutionPolicy Bypass -File tools\make_setup.ps1 -DeviceId ecda3b7c0a04
#
# La clé est lue dans src\secrets.h (MQTT_SECRET) : elle n'a pas à être tapée.
# ATTENTION : le setup.exe contient la clé. Toute personne qui l'a peut commander l'horloge.
param(
	[string]$DeviceId,
	[string]$Broker = 'wss://broker.hivemq.com:8884/mqtt',
	[string]$Secret,
	[string]$Out,
	[switch]$KeepStage   # garde le dossier de préparation (diagnostic ; il contient la clé !)
)
$ErrorActionPreference = 'Stop'
$root  = Split-Path -Parent $PSScriptRoot
$tools = $PSScriptRoot
if (-not $Out) { $Out = Join-Path $root 'dist\Horloge-setup.exe' }

# ---------- Clé et identifiant (depuis secrets.h / config.h si non fournis) ----------
$config = Get-Content (Join-Path $root 'src\config.h') -Raw -Encoding UTF8
$secretsFile = Join-Path $root 'src\secrets.h'   # secrets (exclu de Git)
if (Test-Path $secretsFile) { $config += "`n" + (Get-Content $secretsFile -Raw -Encoding UTF8) }
if (-not $Secret -and $config -match '#define\s+MQTT_SECRET\s+"([^"]*)"') { $Secret = $Matches[1] }
if (-not $Secret -or $Secret -eq 'CHANGEZ_MOI' -or $Secret.Length -lt 16) {
	throw "Clé MQTT_SECRET absente ou trop courte dans src\secrets.h (16 caractères minimum)."
}
if (-not $DeviceId -and $config -match '#define\s+MQTT_DEVICE_ID\s+"([^"]+)"') { $DeviceId = $Matches[1] }
if (-not $DeviceId) { $DeviceId = Read-Host "Identifiant de l'horloge (affiché au démarrage sur le moniteur série)" }
$DeviceId = $DeviceId.Trim().ToLower()
if ($DeviceId -notmatch '^[a-z0-9_-]{1,40}$') { throw "Identifiant invalide : $DeviceId" }

# ---------- Dossier de préparation (supprimé à la fin : il contient la clé) ----------
$stage = Join-Path $env:TEMP ("horloge_setup_" + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $stage | Out-Null
$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
$utf8Bom   = New-Object System.Text.UTF8Encoding($true)
try {
	# ---------- Page pré-configurée ----------
	$html = [IO.File]::ReadAllText((Join-Path $tools 'horloge_ihm.html'), $utf8NoBom)
	$preset = @{ broker = $Broker; devId = $DeviceId; secret = $Secret } | ConvertTo-Json -Compress
	if ($html -notmatch 'const PRESET=null;') { throw "Marqueur 'const PRESET=null;' introuvable dans horloge_ihm.html" }
	$html = $html.Replace('const PRESET=null;', "const PRESET=$preset;")

	# Bibliothèque MQTT embarquée (plus besoin du CDN à chaque ouverture)
	$cdn = 'https://unpkg.com/mqtt@5.10.1/dist/mqtt.min.js'
	try {
		[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
		Invoke-WebRequest -Uri $cdn -OutFile (Join-Path $stage 'mqtt.min.js') -UseBasicParsing
		$html = $html.Replace("<script src=`"$cdn`"></script>", '<script src="mqtt.min.js"></script>')
		Write-Host "Bibliothèque MQTT intégrée"
	} catch {
		Write-Warning "Téléchargement de mqtt.min.js impossible : la page utilisera le CDN (Internet requis)"
	}
	[IO.File]::WriteAllText((Join-Path $stage 'index.html'), $html, $utf8NoBom)

	# ---------- Icône (horloge dessinée, plusieurs tailles, format ICO avec PNG) ----------
	Add-Type -AssemblyName System.Drawing
	$sizes = 16, 32, 48, 256
	$pngs = foreach ($s in $sizes) { # (nom historique : contient PNG ou bitmap selon la taille)
		$bmp = New-Object System.Drawing.Bitmap $s, $s
		$g = [System.Drawing.Graphics]::FromImage($bmp)
		$g.SmoothingMode = 'AntiAlias'
		$g.Clear([System.Drawing.Color]::Transparent)
		$k = $s / 256.0
		$bg = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(255, 27, 32, 39))
		$g.FillEllipse($bg, 8 * $k, 8 * $k, 240 * $k, 240 * $k)
		$ring = New-Object System.Drawing.Pen ([System.Drawing.Color]::FromArgb(255, 255, 77, 61)), ([Math]::Max(1, 16 * $k))
		$g.DrawEllipse($ring, 16 * $k, 16 * $k, 224 * $k, 224 * $k)
		$c = 128 * $k
		$white = [System.Drawing.Color]::FromArgb(255, 232, 235, 239)
		if ($s -ge 32) { # repères des heures
			$dot = New-Object System.Drawing.SolidBrush $white
			for ($h = 0; $h -lt 12; $h++) {
				$a = $h * [Math]::PI / 6; $r = 92 * $k; $d = 12 * $k
				$g.FillEllipse($dot, $c + [Math]::Sin($a) * $r - $d / 2, $c - [Math]::Cos($a) * $r - $d / 2, $d, $d)
			}
		}
		$hand = New-Object System.Drawing.Pen $white, ([Math]::Max(1.5, 18 * $k))
		$hand.StartCap = 'Round'; $hand.EndCap = 'Round'
		$g.DrawLine($hand, $c, $c, $c - 50 * $k, $c - 34 * $k)   # aiguille des heures (10h)
		$hand.Width = [Math]::Max(1.2, 12 * $k)
		$g.DrawLine($hand, $c, $c, $c + 62 * $k, $c - 50 * $k)   # aiguille des minutes (10)
		$g.Dispose()
		$ms = New-Object IO.MemoryStream
		if ($s -ge 256) {
			# 256 px : PNG (format standard pour cette taille)
			$bmp.Save($ms, [System.Drawing.Imaging.ImageFormat]::Png)
		} else {
			# petites tailles : bitmap 32 bits classique (lu partout), lignes de bas en haut + masque
			$bw = New-Object IO.BinaryWriter $ms
			$bw.Write([uint32]40); $bw.Write([int32]$s); $bw.Write([int32]($s * 2))
			$bw.Write([uint16]1); $bw.Write([uint16]32); $bw.Write([uint32]0)
			$bw.Write([uint32]($s * $s * 4)); $bw.Write([int32]0); $bw.Write([int32]0)
			$bw.Write([uint32]0); $bw.Write([uint32]0)
			for ($y = $s - 1; $y -ge 0; $y--) {
				for ($x = 0; $x -lt $s; $x++) {
					$px = $bmp.GetPixel($x, $y)
					$bw.Write([byte]$px.B); $bw.Write([byte]$px.G); $bw.Write([byte]$px.R); $bw.Write([byte]$px.A)
				}
			}
			$maskRow = [int]([Math]::Ceiling($s / 32.0) * 4) # masque AND (vide : la transparence vient de l'alpha)
			$bw.Write((New-Object byte[] ($maskRow * $s)))
			$bw.Flush()
		}
		$bmp.Dispose()
		,$ms.ToArray()
	}
	$ico = New-Object IO.MemoryStream
	$w = New-Object IO.BinaryWriter $ico
	$w.Write([uint16]0); $w.Write([uint16]1); $w.Write([uint16]$sizes.Count)
	$offset = 6 + 16 * $sizes.Count
	for ($i = 0; $i -lt $sizes.Count; $i++) {
		$s = $sizes[$i]; $dim = if ($s -ge 256) { 0 } else { $s }
		$w.Write([byte]$dim); $w.Write([byte]$dim); $w.Write([byte]0); $w.Write([byte]0)
		$w.Write([uint16]1); $w.Write([uint16]32)
		$w.Write([uint32]$pngs[$i].Length); $w.Write([uint32]$offset)
		$offset += $pngs[$i].Length
	}
	foreach ($p in $pngs) { $w.Write($p) }
	$w.Flush()
	[IO.File]::WriteAllBytes((Join-Path $stage 'horloge.ico'), $ico.ToArray())

	# ---------- Scripts d'installation (UTF-8 avec BOM : accents corrects dans PowerShell 5) ----------
	foreach ($f in 'install.ps1', 'uninstall.ps1') {
		$txt = [IO.File]::ReadAllText((Join-Path $tools "installer\$f"), $utf8NoBom)
		[IO.File]::WriteAllText((Join-Path $stage $f), $txt, $utf8Bom)
	}
	[IO.File]::WriteAllText((Join-Path $stage 'install.cmd'),
		"@powershell.exe -NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File `"%~dp0install.ps1`"`r`n",
		[Text.Encoding]::ASCII)

	# ---------- Paquet auto-extractible avec IExpress (fourni avec Windows) ----------
	New-Item -ItemType Directory -Force -Path (Split-Path -Parent $Out) | Out-Null
	if (Test-Path $Out) { Remove-Item $Out -Force }
	$files = @(Get-ChildItem $stage -File | ForEach-Object Name)
	$fileList = @(); $fileRefs = @()
	for ($i = 0; $i -lt $files.Count; $i++) {
		$fileList += 'FILE{0}="{1}"' -f $i, $files[$i]
		$fileRefs += '%FILE{0}%=' -f $i
	}
	$fileList = $fileList -join "`r`n"
	$fileRefs = $fileRefs -join "`r`n"
	# Attention : IExpress échoue sans message si InstallPrompt contient du texte, ou si un texte
	# contient une apostrophe ou un accent. La confirmation est donc faite par install.ps1.
	$sed = @"
[Version]
Class=IEXPRESS
SEDVersion=3
[Options]
PackagePurpose=InstallApp
ShowInstallProgramWindow=1
HideExtractAnimation=1
UseLongFileName=1
InsideCompressed=0
CAB_FixedSize=0
CAB_ResvCodeSigning=0
RebootMode=N
InstallPrompt=%InstallPrompt%
DisplayLicense=%DisplayLicense%
FinishMessage=%FinishMessage%
TargetName=%TargetName%
FriendlyName=%FriendlyName%
AppLaunched=%AppLaunched%
PostInstallCmd=%PostInstallCmd%
AdminQuietInstCmd=%AdminQuietInstCmd%
UserQuietInstCmd=%UserQuietInstCmd%
SourceFiles=SourceFiles
[Strings]
InstallPrompt=
DisplayLicense=
FinishMessage=
TargetName=$Out
FriendlyName=Horloge
AppLaunched=cmd.exe /c install.cmd
PostInstallCmd=<None>
AdminQuietInstCmd=
UserQuietInstCmd=
$fileList
[SourceFiles]
SourceFiles0=$stage\
[SourceFiles0]
$fileRefs
"@
	$sedPath = Join-Path $stage 'horloge.sed'
	[IO.File]::WriteAllText($sedPath, $sed, [Text.Encoding]::Default)
	# IExpress échoue (code 1, sans message) si le chemin du SED contient des '\' : on passe des '/'
	$p = Start-Process -FilePath "$env:WINDIR\System32\iexpress.exe" -ArgumentList '/N', '/Q', ($sedPath -replace '\\', '/') -Wait -PassThru
	if (-not (Test-Path $Out)) { throw "IExpress n'a pas produit $Out (code $($p.ExitCode))" }
	Write-Host ""
	Write-Host "Créé : $Out" -ForegroundColor Green
	Write-Host "Horloge : $DeviceId   Broker : $Broker"
	Write-Host "ATTENTION : ce fichier contient la clé secrète. Ne le donnez qu'aux personnes autorisées." -ForegroundColor Yellow
}
finally {
	if ($KeepStage) { Write-Host "Dossier de préparation conservé : $stage" } else { Remove-Item $stage -Recurse -Force -ErrorAction SilentlyContinue }
}
