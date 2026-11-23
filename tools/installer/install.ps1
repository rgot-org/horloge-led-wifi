# Installation de la télécommande de l'horloge (lancé par Horloge-setup.exe)
# - copie la page dans %LOCALAPPDATA%\HorlogeTelecommande (aucun droit administrateur nécessaire)
# - icône sur le Bureau et dans le menu Démarrer (ouverture en fenêtre d'application Edge)
# - entrée dans « Applications installées » pour la désinstallation
$ErrorActionPreference = 'Stop'
$ws = New-Object -ComObject WScript.Shell
if ($ws.Popup("Installer la télécommande de l'horloge sur cet ordinateur ?", 0, "Horloge", 4 + 32) -ne 6) { exit }
try {
	$src  = Split-Path -Parent $MyInvocation.MyCommand.Path
	$dest = Join-Path $env:LOCALAPPDATA 'HorlogeTelecommande'
	New-Item -ItemType Directory -Force -Path $dest | Out-Null
	foreach ($f in 'index.html', 'mqtt.min.js', 'horloge.ico', 'uninstall.ps1') {
		$p = Join-Path $src $f
		if (Test-Path $p) { Copy-Item $p -Destination $dest -Force }
	}
	$html = Join-Path $dest 'index.html'
	$uri  = ([Uri]$html).AbsoluteUri
	$icon = Join-Path $dest 'horloge.ico'

	# Navigateur capable d'ouvrir une « fenêtre d'application » (sans barre d'adresse) :
	# Edge, sinon Chrome. A défaut, la page s'ouvre dans le navigateur par défaut (Firefox n'a pas ce mode).
	function Find-Browser([string]$exe, [string[]]$folders) {
		# 1) registre « App Paths », en vue 64 bits ET 32 bits (le script peut tourner en 32 bits)
		foreach ($hive in [Microsoft.Win32.RegistryHive]::CurrentUser, [Microsoft.Win32.RegistryHive]::LocalMachine) {
			foreach ($view in [Microsoft.Win32.RegistryView]::Registry64, [Microsoft.Win32.RegistryView]::Registry32) {
				try {
					$base = [Microsoft.Win32.RegistryKey]::OpenBaseKey($hive, $view)
					$k = $base.OpenSubKey("SOFTWARE\Microsoft\Windows\CurrentVersion\App Paths\$exe")
					if ($k) { $p = $k.GetValue(''); $k.Close(); if ($p -and (Test-Path $p)) { return $p } }
				} catch { }
			}
		}
		# 2) dossiers d'installation habituels
		foreach ($f in $folders) {
			if ($f) { $p = Join-Path $f $exe; if (Test-Path $p) { return $p } }
		}
		return $null
	}
	$pf = $env:ProgramFiles; $pf86 = ${env:ProgramFiles(x86)}; $pfw = $env:ProgramW6432
	$browser = Find-Browser 'msedge.exe' @("$pf86\Microsoft\Edge\Application", "$pf\Microsoft\Edge\Application", "$pfw\Microsoft\Edge\Application", "$env:LOCALAPPDATA\Microsoft\Edge\Application")
	if (-not $browser) {
		$browser = Find-Browser 'chrome.exe' @("$pf\Google\Chrome\Application", "$pfw\Google\Chrome\Application", "$pf86\Google\Chrome\Application", "$env:LOCALAPPDATA\Google\Chrome\Application")
	}

	function New-AppShortcut([string]$path) {
		$s = $ws.CreateShortcut($path)
		if ($browser) {
			$s.TargetPath = $browser
			$s.Arguments  = "--app=`"$uri`" --window-size=1200,850"
		} else {
			$s.TargetPath = $html
		}
		$s.WorkingDirectory = $dest
		$s.IconLocation     = "$icon,0"
		$s.Description      = "Télécommande de l'horloge"
		$s.Save()
	}
	$desktopLnk = Join-Path ([Environment]::GetFolderPath('Desktop'))  'Horloge.lnk'
	$startLnk   = Join-Path ([Environment]::GetFolderPath('Programs')) 'Horloge.lnk'
	New-AppShortcut $desktopLnk
	New-AppShortcut $startLnk

	# Désinstallation depuis Paramètres > Applications > Applications installées
	$reg = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\HorlogeTelecommande'
	New-Item -Path $reg -Force | Out-Null
	Set-ItemProperty $reg DisplayName     "Horloge (télécommande)"
	Set-ItemProperty $reg DisplayIcon     $icon
	Set-ItemProperty $reg Publisher       "Horloge"
	Set-ItemProperty $reg InstallLocation $dest
	Set-ItemProperty $reg UninstallString "powershell.exe -NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File `"$dest\uninstall.ps1`""
	Set-ItemProperty $reg NoModify 1 -Type DWord
	Set-ItemProperty $reg NoRepair 1 -Type DWord

	$ws.Popup("La télécommande de l'horloge est installée.`n`nUne icône « Horloge » a été placée sur le Bureau.", 0, "Horloge", 64) | Out-Null
	Start-Process -FilePath $desktopLnk
}
catch {
	$ws.Popup("L'installation a échoué :`n`n$($_.Exception.Message)", 0, "Horloge", 16) | Out-Null
	exit 1
}
