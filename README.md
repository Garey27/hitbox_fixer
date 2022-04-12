# Hitbox Fixer ![C/C++ CI](https://github.com/Garey27/hitbox_fixer/actions/workflows/cmake.yml/badge.svg)
This module fixes incorrect player server-side hitboxes in Counter-Strike 1.6.
## Requirements
* [REHLDS](https://github.com/dreamstalker/rehlds/releases) version 3.11 or above 
## Demonstration of bugs and fixes (youtube video)
[![CS 1.6 Hitboxes Bugs and Fixes](https://img.youtube.com/vi/gPCN_6aXl54/0.jpg)](https://www.youtube.com/watch?v=gPCN_6aXl54 "CS 1.6 Hitboxes Bugs and Fixes")
## Fixes
- [x] Fixes absolutely broken hitboxes when numblends == 1 (ducking/standing in reload weapon or plant c4 animation).
- [x] Hitbox backtrack based on client-side position.
- [x] Fixes broken hitbox position on spawn (since of correct backtrack)
## Downloads
* [Release builds](https://github.com/Garey27/hitbox_fixer/releases)
## TODO
- [ ] Add HLDS compatibility (need to hook client_t* struct)
- [ ] Fully implement non-players setupbones code.
