# Changelog

## v0.2 (2026-07-07)

### Emulation improvements
- CPU: Fix fir\_mov logic for MULS instruction
- PPU: Improve blending over transparent pixels
- PPU: Draw blended sprites in depth layer order
- SPU: Support wave-in software mix channel
- SPG200: Add watchdog emulation

### UI changes
- Add SPU output visualization window
- Add memory editor
- Add support for selecting and loading ROMs from UI
- Add setting for allowing background gamepad input
- Fix border artifacts in bilinear fullscreen mode
- Ignore keyboard presses for controller input if ImGui window has focus
- Add icon, FreeDesktop desktop entry file
- Add About window
- Disable imgui.ini

## v0.1 (2025-03-15)

- Initial release
