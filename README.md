# hitbox_fixer [C/C++ CI](https://github.com/Garey27/hitbox_fixer/.github/workflows/cmake.yml/badge.svg)

This module fixes incorrect player server-side hitboxes in Counter-Strike 1.6.

## Requirements

* REHLDS version 3.11+

## Fixes

- [x] Fixes absolutely broken hitboxes when numblends == 1 (ducking/standing in reload weapon or plant c4 animation).
- [x] Hitbox backtrack based on client-side position.
- [x] Fixes broken hitbox position on spawn (since of correct backtrack)

## TODO
- [ ] Add HLDS compatibility (need to hook client_t* struct)
- [ ] Add Cvars to Enable/Disable fixes
- [ ] Fully implement non-players setupbones code.
