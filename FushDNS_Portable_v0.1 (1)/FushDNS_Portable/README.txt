Fush DNS Portable v0.1 — Chrome-only domain blocking, not a DNS proxy.
1. Open FushDNS_Portable/FushDNS_Portable.ino in Arduino IDE.
2. Select ESP32C3 Dev Module, USB CDC On Boot Enabled, and your C3 COM port. Upload. Close Serial Monitor.
3. On a personal Chrome installation that permits extensions, visit chrome://extensions, enable Developer mode and Load unpacked, choosing the extension folder.
4. Click Fush DNS Portable in the extensions menu, then Connect C3 & enable. Select the C3 serial port. The board sends eight domain names; Chrome installs blocking rules.
5. Disable blocking from the dashboard to remove rules. Rules persist after unplugging or closing the dashboard. Disable/uninstall extension to stop it too.
No Wi-Fi credentials, Windows DNS changes or companion executable. Does not affect other browsers or apps. No YouTube video ad blocking. Do not bypass managed browser restrictions. Not compiled or hardware/browser tested. 
