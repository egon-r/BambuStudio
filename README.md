![image](https://user-images.githubusercontent.com/106916061/179006347-497d24c0-9bd6-45b7-8c49-d5cc8ecfe5d7.png)
# Bambu Studio — egon-r fork

Personal fork of [bambulab/BambuStudio](https://github.com/bambulab/BambuStudio)
tracking upstream `master`. The integration branch is `egon-r`; individual
features live on `feat/*` / `fix/*` branches. Everything below the next
section is the upstream README.

## Fork differences

| Change | Commit | Notes |
|---|---|---|
| **Multi-wall spiral vase** (experimental) | `190e114c`, `1b3f751e`, `332b9643f` | New `spiral_mode_multi_wall` option: with spiral vase on, `wall_loops` may exceed 1 and each wall prints as its own continuous helix of pitch `layer_height`, walls phased `h/N` apart in Z, nozzle hops Z down between walls. Safe per-layer fallback (layer emitted unmodified if run count ≠ wall count). Hardware-verified on an A1 mini (2 walls, PLA): prints continuously with no visible seams or wall-transition artifacts. `1b3f751e` + `332b9643f` fix segfaults when loading 3mfs whose embedded presets predate the fork (missing key). |
| **Woven walls** (experimental) | `a33ba1b1f` | New `spiral_mode_woven` (+ `spiral_mode_weave_amplitude`, `spiral_mode_weave_cells_per_lap`) on top of multi-wall spiral vase: the wall helices' Z phases are sinusoidally modulated in antiphase, so neighboring walls periodically converge into re-melted weld joints and separate again — the woven joint pattern observed by accident at vase corners, made deliberate and distributed around the whole perimeter. Extruding moves are subdivided to half-cell length so the weave resolves on coarse polygons; amplitude is clamped below `h/(2N)` so walls never swap order. Slicer-verified: wall gap oscillates over the full `[h/2−2A, h/2+2A]` range at every angle, per-lap climb stays exactly one layer height, and weave-off output is byte-identical (wall moves) to the multi-wall baseline. **Not yet hardware-verified.** |
| **Raw `.gcode` viewer + printing** | `bc4d6fae`, `893fc7df7`, `ed4c8447a`, `cf41600d3` | Restores File → Open G-code… (`Ctrl+Shift+O`; BBL `#if 0`'d the viewer menu), a raw gcode file now reports its used filaments so the send dialog's AMS mapping UI works instead of blocking with "Not all filaments used in slicing are mapped to the printer", and Print on a gcode file opens the normal SelectMachineDialog instead of BBL's "does not support sending .gcode files to print" storage-only note dialog (that dialog appeared whenever the only-gcode flag hadn't been cleared by a background config re-apply). SendJob already handles gcode-mode transport; the flag-timing dependence is gone. |
| Plain-text studio logs | `48a84d1d` | Studio logs under `<datadir>/log/` are written as plain text instead of encrypted `.enc` files. |
| Gyroid sparse infill guardrails | `44316ec7`, `c2b5a3ef1` | Slice-time warning when gyroid sparse infill is selected; BBL process presets default to grid instead of gyroid. |
| Vibration Compensation toggle | `1337b4478` | Toggle in the send/print dialog. |
| TreeSupport enforcer bounds fix | `a3c09c93a` | Bounds-checks enforcers before per-layer access (crash fix). |

---

# Upstream README

Bambu Studio is a cutting-edge, feature-rich slicing software.  
It contains project-based workflows, systematically optimized slicing algorithms, and an easy-to-use graphic interface, bringing users an incredibly smooth printing experience.

Prebuilt Windows, macOS 64-bit and Linux releases are available through the [github releases page](https://github.com/bambulab/BambuStudio/releases/).

Bambu Studio is based on [PrusaSlicer](https://github.com/prusa3d/PrusaSlicer) by Prusa Research, which is from [Slic3r](https://github.com/Slic3r/Slic3r) by Alessandro Ranellucci and the RepRap community.

See the [wiki](https://github.com/bambulab/BambuStudio/wiki) and the [documentation directory](https://github.com/bambulab/BambuStudio/tree/master/doc) for more information.

# What are Bambu Studio's main features?
Key features are:
- Basic slicing features & GCode viewer
- Multiple plates management
- Remote control & monitoring
- Auto-arrange objects
- Auto-orient objects
- Hybrid/Tree/Normal support types, Customized support
- multi-material printing and rich painting tools
- multi-platform (Win/Mac/Linux) support
- Global/Object/Part level slicing parameters

Other major features are:
- Advanced cooling logic controlling fan speed and dynamic print speed
- Auto brim according to mechanical analysis
- Support arc path(G2/G3)
- Support STEP format
- Assembly & explosion view
- Flushing transition-filament into infill/object during filament change

# How to compile
Following platforms are currently supported to compile:
- Windows 64-bit, [Compile Guide](https://github.com/bambulab/BambuStudio/wiki/Windows-Compile-Guide)
- Mac 64-bit, [Compile Guide](https://github.com/bambulab/BambuStudio/wiki/Mac-Compile-Guide)
- Linux, [Compile Guide](https://github.com/bambulab/BambuStudio/wiki/Linux-Compile-Guide)
  - currently we only provide linux appimages on [github releases](https://github.com/bambulab/BambuStudio/releases) for Ubuntu/Fedora, and a [flathub version](https://flathub.org/apps/com.bambulab.BambuStudio) can be used for all the linux platforms

# Report issue
You can add an issue to the [github tracker](https://github.com/bambulab/BambuStudio/issues) if **it isn't already present.**

# License
Bambu Studio is licensed under the GNU Affero General Public License, version 3. Bambu Studio is based on PrusaSlicer by PrusaResearch.

PrusaSlicer is licensed under the GNU Affero General Public License, version 3. PrusaSlicer is owned by Prusa Research. PrusaSlicer is originally based on Slic3r by Alessandro Ranellucci.

Slic3r is licensed under the GNU Affero General Public License, version 3. Slic3r was created by Alessandro Ranellucci with the help of many other contributors.

The GNU Affero General Public License, version 3 ensures that if you use any part of this software in any way (even behind a web server), your software must be released under the same license.

The bambu networking plugin is based on non-free libraries. It is optional to the Bambu Studio and provides extended networking functionalities for users.
By default, after installing Bambu Studio without the networking plugin, you can initiate printing through the SD card after slicing is completed.

