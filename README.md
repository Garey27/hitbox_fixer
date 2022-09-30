# Hitbox Fixer ![C/C++ CI](https://github.com/Garey27/hitbox_fixer/workflows/CMake/badge.svg)
This module fixes incorrect player server-side hitboxes in Counter-Strike 1.6, Half-Life and AG. (Create issue if want support for your mod).
## Requirements
* [HLDS] version 8648
or
* [REHLDS](https://github.com/dreamstalker/rehlds/releases) version 3.3 or above 
## Fixes
- [x] Fixes absolutely broken hitboxes when numblends == 1 (ducking/standing in reload weapon or plant c4 animation).
- [x] Hitbox backtrack based on client-side position.
- [x] Fixes broken hitbox position on spawn (since of correct backtrack)
## Downloads
* [Release builds](https://github.com/Garey27/hitbox_fixer/releases)
## TODO
- [ ] Fully implement non-players setupbones code.
