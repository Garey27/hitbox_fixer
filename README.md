# Hitbox Fixer ![C/C++ CI](https://github.com/Garey27/hitbox_fixer/workflows/CMake/badge.svg)
This module fixes incorrect player server-side hitboxes in Counter-Strike 1.6.
## Requirements
* [REHLDS](https://github.com/dreamstalker/rehlds/releases) version 3.11 or above 
## Fixes
- [x] Fixes absolutely broken hitboxes when numblends == 1 (ducking/standing in reload weapon or plant c4 animation).
- [x] Hitbox backtrack based on client-side position.
- [x] Fixes broken hitbox position on spawn (since of correct backtrack)
## Downloads
* [Release builds](https://github.com/Garey27/hitbox_fixer/releases)
## TODO
- [ ] Add HLDS compatibility (need to hook client_t* struct)
- [ ] Fully implement non-players setupbones code.
