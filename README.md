# veesem 

veesem is an experimental V.Smile emulator. Game compatibility is generally quite good from what I have tested so far, though more features, a more advanced UI and better accuracy are future targets.

## Command line options

* Usage: `veesem [OPTIONS] CARTROM`
* Emulation options:
    * `-sysrom ROM` - Provide system ROM (otherwise dummy ROM without boot animation will be used)
    * `-pal` - Use PAL video timing (default)
    * `-ntsc` - Use NTSC video timing
* Visual options:
    * `-leds` - Show controller LEDs at startup
    * `-fps` - Show emulation FPS at startup

## Controls

Currently only the standard V.Smile controller is supported.

| V.Smile  | Keyboard   | Gamepad (Xbox/SDL_GameController) |
| ------   | --------   | ----------------------------------|
| Enter    | Space      | A                                 |
| Help     | A          | B                                 |
| Back     | S          | Start                             |
| ABC      | D          | Select/Back                       |
| Red      | Z          | X                                 |
| Yellow   | X          | Y                                 |
| Blue     | C          | RB                                |
| Green    | V          | LB                                |
| Joystick | Arrow keys | D-pad or left analog stick



