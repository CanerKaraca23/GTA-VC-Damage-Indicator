# Damage Indicator for GTA Vice City (v1.0)

[![Game: GTA Vice City](https://img.shields.io/badge/Game-GTA%20Vice%20City%20v1.0-orange.svg)](https://en.wikipedia.org/wiki/Grand_Theft_Auto:_Vice_City)
[![Type: ASI Plugin](https://img.shields.io/badge/Type-ASI%20Plugin-blue.svg)]()
[![Platform: PC](https://img.shields.io/badge/Platform-PC%20x86-brightgreen.svg)]()
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

A lightweight, clean, and minimal native C++ ASI plugin for **Grand Theft Auto: Vice City (v1.0)** that displays smooth directional screen-edge damage indicators.

---

## ✨ Features

- 🔴 **Soft Screen-Edge Glow**: Alpha-blended Gouraud gradient on the screen perimeter indicating the exact direction of incoming attacks with smooth corner-tapered transitions.
- 🔄 **Real-Time 360° Camera Tracking**: Fully compatible with free-camera modifications such as `ClassicAxisVC.asi` and widescreen fixes. When rotating the camera, the indicator dynamically updates its position along the screen edges in real time.
- ⚡ **Zero External Dependencies**: Pure native C++ utilizing RenderWare's native 2D immediate mode pipeline (`RwIm2DVertex` / `RwIm2DRenderPrimitive`).
- 🛡️ **Crash-Safe Hooking**: Naked assembly trampoline with SEH protection ensuring 100% stability without interfering with other ASI plugins or CLEO scripts.

---

## 🎮 Mechanics

Whenever Tommy takes damage:
1. The attacker's world coordinates are captured at impact.
2. Every frame, the attack vector is dynamically projected onto the active camera's 2D line-of-sight and right vectors.
3. The corresponding screen edge lights up in smooth blood red and fades out over 750ms with quadratic decay ($t^2$).
4. If the camera turns (e.g. via Classic Axis mouse look), the glow dynamically follows the camera rotation in real time.
5. Non-directional damage (explosions, fall, fire) triggers a uniform vignette pulse across the screen.

---

## 📥 Installation

1. Make sure you have an ASI Loader installed (e.g., [Ultimate ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader) or Silent's ASI Loader).
2. Download `DamageIndicator.asi` from the [Releases](https://github.com/CanerKaraca23/GTA-VC-Damage-Indicator/releases) tab.
3. Place `DamageIndicator.asi` into your GTA Vice City `scripts\` or root game directory.
4. Launch the game and enjoy!

---

## 🛠️ Building from Source

### Requirements
- Microsoft Visual C++ Compiler (MSVC x86 / 32-bit) via Visual Studio or Build Tools
- Windows SDK

### Build Steps
1. Clone the repository:
   ```bash
   git clone https://github.com/CanerKaraca23/GTA-VC-Damage-Indicator.git
   ```
2. Open a 32-bit Developer Command Prompt or simply run `build.bat`:
   ```cmd
   cl /nologo /O2 /MD /LD /EHsc src\DamageIndicator.cpp /Febin\DamageIndicator.asi /link /INCREMENTAL:NO user32.lib kernel32.lib
   ```

---

## 📜 Credits & Acknowledgments
- **Caner Karaca** - Author & Developer
- **Rockstar Games** - Grand Theft Auto: Vice City
- **DK22Pac & GTA Community** - Reverse-engineering research and Plugin-SDK
