# Damage Indicator for GTA Vice City (v1.0)

[![Game: GTA Vice City](https://img.shields.io/badge/Game-GTA%20Vice%20City%20v1.0-orange.svg)](https://en.wikipedia.org/wiki/Grand_Theft_Auto:_Vice_City)
[![Type: ASI Plugin](https://img.shields.io/badge/Type-ASI%20Plugin-blue.svg)]()
[![Platform: PC](https://img.shields.io/badge/Platform-PC%20x86-brightgreen.svg)]()
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

A lightweight, clean, and minimal native C++ ASI plugin for **Grand Theft Auto: Vice City (v1.0)** that displays smooth directional screen-edge damage indicators with full `.ini` customization.

---

## ✨ Features

- 🔴 **Soft Screen-Edge Glow**: Alpha-blended Gouraud gradient on the screen perimeter indicating the exact direction of incoming attacks with smooth corner transitions.
- ⚙️ **Fully Customizable via `DamageIndicator.ini`**: Easily configure color (RGB), peak opacity, duration, edge thickness, and fade curve.
- 🔄 **Real-Time 360° Camera Tracking**: Fully compatible with free-camera modifications such as `ClassicAxisVC.asi` and widescreen fixes. When rotating the camera, the indicator dynamically updates its position along the screen edges in real time.
- ⚡ **Zero External Dependencies**: Pure native C++ utilizing RenderWare's native 2D immediate mode pipeline (`RwIm2DVertex` / `RwIm2DRenderPrimitive`).
- 🛡️ **Crash-Safe Hooking**: Naked assembly trampoline with SEH protection ensuring 100% stability without interfering with other ASI plugins or CLEO scripts.

---

## ⚙️ Configuration (`DamageIndicator.ini`)

The mod creates a `DamageIndicator.ini` file in the same directory as the `.asi` file upon first launch if not already present.

```ini
; ==============================================================================
; GTA Vice City - Damage Indicator Configuration by Caner Karaca
; ==============================================================================

[General]
; Enable or disable the damage indicator (1 = Enabled, 0 = Disabled)
Enabled = 1

; Optional hotkey to reload settings without restarting the game
; (0 = Disabled, 121 = F10, 122 = F11, 123 = F12)
ReloadKey = 0

[Color]
; Indicator color in RGB format (0 - 255)
Red = 215
Green = 18
Blue = 18

[Appearance]
; Maximum edge opacity at peak damage (0 - 255, default 210)
MaxAlpha = 210

; Duration of the indicator fade in milliseconds (default 750)
Duration = 750

; Side edge thickness (% of screen width, 1 - 50, default 16)
WidthPercent = 16

; Top/Bottom edge thickness (% of screen height, 1 - 50, default 18)
HeightPercent = 18

; Fade-out animation curve (1 = Smooth quadratic, 0 = Linear)
SmoothFade = 1
```

---

## 🎮 Mechanics

Whenever Tommy takes damage:
1. The attacker's world coordinates are captured at impact.
2. Every frame, the attack vector is dynamically projected onto the active camera's 2D line-of-sight and right vectors.
3. The corresponding screen edge lights up in smooth color gradient and fades out smoothly.
4. If the camera turns (e.g. via Classic Axis mouse look), the glow dynamically follows the camera rotation in real time.
5. Non-directional damage (explosions, fall, fire) triggers a uniform vignette pulse across the screen.

---

## 📥 Installation

1. Make sure you have an ASI Loader installed (e.g., [Ultimate ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader) or Silent's ASI Loader).
2. Download `DamageIndicator.asi` and `DamageIndicator.ini` from the [Releases](https://github.com/CanerKaraca23/GTA-VC-Damage-Indicator/releases) tab.
3. Place `DamageIndicator.asi` and `DamageIndicator.ini` into your GTA Vice City `scripts\` or root game directory.
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
   build.bat
   ```

---

## 📜 Credits & Acknowledgments
- **Caner Karaca** - Author & Developer
- **Plugin-SDK** - Reverse-engineering research and SDK
