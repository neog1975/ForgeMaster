**Forge Master** – free, open-source one-click mastering plugin (VST3 / AU / Standalone).

## Download

Under **Assets** below:

- **Windows:** `ForgeMaster-Windows.zip`
- **macOS** (Apple Silicon + Intel): `ForgeMaster-macOS.zip`

## Install

### Windows
1. Unzip `ForgeMaster-Windows.zip`.
2. Copy `Forge Master.vst3` to `C:\Program Files\Common Files\VST3`.
3. In your DAW, rescan plugins (FL Studio: *Options → Manage plugins → Find plugins*).

### macOS
1. Unzip `ForgeMaster-macOS.zip`.
2. Copy `Forge Master.vst3` to `/Library/Audio/Plug-Ins/VST3`
   and `Forge Master.component` (AU, for Logic/GarageBand) to `/Library/Audio/Plug-Ins/Components`.
3. The plugin is not yet signed by Apple, so macOS may block it. Open **Terminal** and run:
   ```
   sudo xattr -dr com.apple.quarantine "/Library/Audio/Plug-Ins/VST3/Forge Master.vst3" "/Library/Audio/Plug-Ins/Components/Forge Master.component"
   ```
4. Restart your DAW and rescan plugins.

`Forge Master.app` / `Forge Master.exe` is the standalone version – it runs without a DAW.

---

## Instalimi (Shqip)

**Windows:** shpaketo ZIP-in dhe kopjo `Forge Master.vst3` te `C:\Program Files\Common Files\VST3`, pastaj bëj rescan të plugin-ave në DAW (FL Studio: *Options → Manage plugins → Find plugins*).

**macOS:** shpaketo ZIP-in, kopjo `Forge Master.vst3` te `/Library/Audio/Plug-Ins/VST3` dhe `Forge Master.component` te `/Library/Audio/Plug-Ins/Components`. Nëse macOS e bllokon, ekzekuto komandën `sudo xattr …` nga lart në Terminal dhe rinise DAW-in.

Feedback, bug reports and pull requests are welcome!
