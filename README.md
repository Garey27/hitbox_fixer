# Hitbox Fixer ![C/C++ CI](https://github.com/Garey27/hitbox_fixer/workflows/CMake/badge.svg)

This module addresses and resolves incorrect server-side hitboxes for players in **Counter-Strike 1.6**, **Half-Life**, and **Adrenaline Gamer (AG)**.  
Feel free to create an issue if you want support for additional mods.

---

## Requirements

One of the following server versions is required:
- **HLDS** version `8648`  
- [**REHLDS**](https://github.com/rehlds/rehlds/ReHLDS) version `3.10` or above  

---

## Features & Fixes

- [x] **Fixed broken hitboxes when `numblends == 1`:**  
  - Issues with animations such as crouching, standing, reloading, or planting C4 are now resolved.
- [x] **Hitbox backtrack based on client-side positions:**  
  - Ensures smoother and more accurate hitbox synchronization between server and client.
- [x] **Fixed hitbox position on spawn:**  
  - Resolved issues caused by incorrect backtracking logic, ensuring hitboxes are calculated correctly at spawn time.

---

## Downloads

Get the latest release builds here:  
➡️ [**Release Builds**](https://github.com/Garey27/hitbox_fixer/releases)

---

## TODO

- [ ] **Fully implement `setupbones` logic for non-player entities.**

---

### Changelog (Alpha Update):

- Moved animation code from `AddToFullpack` to `Prethink` + `AddToFullpack` for improved accuracy in simulating client-side behavior.
- Code for `StudioEstimateGait` is now aligned with the **CS 1.6 client**, significantly reducing desynchronization on player spawn. However, exact `frametime` replication is not feasible.
- Enhanced handling of `ex_interp`.
- Added **hitbox visualizer** as a `.asi` plugin:
  - Currently supports only **one player** and is configured for **Counter-Strike**.

#### Using the Visualizer:
1. Launch **Half-Life/CS** and **HLDS** with the `-insecure` flag.
2. Copy the `hitbox_vis.asi` file to the Half-Life folder.
3. Start the game and connect to a server.
4. On the server, set the following cvars:
   - `hbf_debug 1` for basic animation parameter output.
   - `hbf_debug 2` for detailed debugging (shows hitboxes exactly as calculated by the server).
