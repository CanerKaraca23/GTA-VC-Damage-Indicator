# GTA V Style Damage Indicator for GTA Vice City (v1.0)

[![Game: GTA Vice City](https://img.shields.io/badge/Game-GTA%20Vice%20City%20v1.0-orange.svg)](https://en.wikipedia.org/wiki/Grand_Theft_Auto:_Vice_City)
[![Type: ASI Plugin](https://img.shields.io/badge/Type-ASI%20Plugin-blue.svg)]()
[![Platform: PC](https://img.shields.io/badge/Platform-PC%20x86-brightgreen.svg)]()
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

A lightweight, native C++ ASI plugin for **Grand Theft Auto: Vice City (v1.0)** that brings modern **Grand Theft Auto V-style directional damage feedback** into the classic RenderWare 3D engine.

---

## ✨ Features

- 🎯 **Curved Crescent Reticle Arc**: Multi-segment Gouraud-shaded crescent arc around the screen center/crosshair pointing directly toward the incoming damage source.
- 🔴 **Soft Screen-Edge Glow**: Alpha-blended Gouraud gradient on the screen perimeter indicating the direction of attack with corner-tapered fading.
- 🩸 **Low-Health Blood Vignette**: Dynamic blood pulse effect covering the screen borders when Tommy's health falls below 25%.
- 🔄 **360° Camera Line-of-Sight Support**: Fully compatible with free-camera modifications such as `ClassicAxisVC.asi` and widescreen fixes.
- ⚡ **Zero External Dependencies**: Pure native C++ utilizing RenderWare's native 2D immediate mode pipeline (`RwIm2DVertex` / `RwIm2DRenderPrimitive`).
- 🛡️ **Crash-Safe Hooking**: Exception-handled SEH trampolines ensuring 100% stability without interfering with other ASI plugins or CLEO scripts.

---

## 🎮 Preview & Mechanics

Whenever the player takes damage:
1. The attacker's entity coordinates are captured and projected onto the active camera's 2D line-of-sight vector.
2. A curved red arc appears around the reticle and fades out smoothly over 750ms with quadratic decay ($t^2$).
3. The screen edge corresponding to the attacker's direction lights up with a soft red glow.
4. If damage is non-directional (e.g. falling, fire, explosion), a uniform vignette pulse triggers across the screen.

---

## 📥 Installation

1. Make sure you have an ASI Loader installed (e.g., [Ultimate ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader) or Silent's ASI Loader).
2. Download `GTAVDamageIndicator.asi` from the [Releases](https://github.com/CanerKaraca23/GTA-VC-Damage-Indicator/releases) tab.
3. Place `GTAVDamageIndicator.asi` into your GTA Vice City `scripts\` or root game directory.
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
2. Open a 32-bit Visual Studio Developer Command Prompt (`vcvars32.bat`):
   ```cmd
   cl /nologo /O2 /MD /LD /EHsc src\GTAVDamageIndicator.cpp /Fe:GTAVDamageIndicator.asi /link /INCREMENTAL:NO user32.lib kernel32.lib
   ```
   Or simply run `build.bat`.

---

## 📜 Credits & Acknowledgments
- **Caner Karaca** - Author & Developer
- **Rockstar Games** - Grand Theft Auto: Vice City & GTA V
- **DK22Pac & GTA Community** - Reverse-engineering research and Plugin-SDK
