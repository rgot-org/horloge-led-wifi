# Désinstallation de la télécommande de l'horloge
$ws = New-Object -ComObject WScript.Shell
if ($ws.Popup("Supprimer la télécommande de l'horloge de cet ordinateur ?", 0, "Horloge", 4 + 32) -ne 6) { exit }

$dest = Join-Path $env:LOCALAPPDATA 'HorlogeTelecommande'
Remove-Item (Join-Path ([Environment]::GetFolderPath('Desktop'))  'Horloge.lnk') -ErrorAction SilentlyContinue
Remove-Item (Join-Path ([Environment]::GetFolderPath('Programs')) 'Horloge.lnk') -ErrorAction SilentlyContinue
Remove-Item 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\HorlogeTelecommande' -Recurse -ErrorAction SilentlyContinue

# Ce script est dans le dossier à supprimer : la suppression est faite juste après sa fin
Start-Process -WindowStyle Hidden -FilePath cmd.exe -ArgumentList "/c timeout /t 2 /nobreak >nul & rmdir /s /q `"$dest`""
$ws.Popup("La télécommande a été supprimée.", 0, "Horloge", 64) | Out-Null
