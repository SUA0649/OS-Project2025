# Get WSL IP
$wslIP = (wsl hostname -I).Trim()

# Configure port proxy for P2P ports
8000, 9000, 9100 | ForEach-Object {
    netsh interface portproxy add v4tov4 listenport=$_ listenaddress=0.0.0.0 connectport=$_ connectaddress=$wslIP
}

# Allow ports through Windows Firewall
8000, 9000, 9100 | ForEach-Object {
    New-NetFirewallRule -DisplayName "WSL P2P Port $_" -Direction Inbound -Action Allow -Protocol TCP -LocalPort $_
}

Write-Host "Port proxy and firewall rules configured for WSL P2P."