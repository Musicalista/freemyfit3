@echo off
rem Fit3 secure proxy: the watch reaches this PC through the phone's Bluetooth tethering (the phone must be on the same Wi-Fi as this PC).
rem First time only, allow the port through the Windows firewall (run once in an Administrator prompt):
rem   netsh advfirewall firewall add rule name="Fit3 proxy" dir=in action=allow protocol=TCP localport=8788
cd /d "%~dp0"
node proxy.js 8788
pause
