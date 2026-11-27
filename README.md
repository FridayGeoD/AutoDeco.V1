# Auto Deco — Geode mod for Geometry Dash

Auto Deco adds a **Deco** button to the level editor. It reads the layout of your level and fills it with decoration in one click:

| Layer | What it does |
|---|---|
| **Edge glow** | Puts glow pieces on every exposed side of your solid blocks and rotates each one to face outward |
| **Ground deco** | Puts grass, spike deco and similar pieces on top of floors and platforms |
| **Hanging deco** | Hangs chains, vines and similar pieces under ceilings |
| **Background deco** | Puts larger pieces behind your blocks on the B2 Z layer |
| **Air particles** | Scatters small pieces through the empty space over the level |

The output is **deterministic**: the same seed gives the same decoration every time. Press **Rnd** to roll a new seed.

## Install & build

You need a compiled `.geode` file. The easiest way to get one is to let GitHub build it, so you don't have to install anything:

1. Create a new GitHub repo and upload everything in this folder, including the `.github` folder.
2. Open the **Actions** tab. The *Build Geode Mod* workflow runs on every push.
3. When it finishes, download the **Build Output** artifact. It contains `yourname.auto-deco.geode`.
4. Put that file in your Geometry Dash `geode/mods` folder, or install it from a file in the Geode mods menu. Then restart GD.

**Local build (optional):** install the [Geode CLI and SDK](https://docs.geode-sdk.org/getting-started/), then run `geode build` in this folder.

### Version note
`mod.json` targets **Geode 5.0.0 / GD 2.2081**. If your Geode or GD version is different, change the `"geode"` and `"gd"` fields to match. You can see your versions in the Geode menu in game.
Change `id` and `developer` to your own name before you publish anything.

## Usage

1. Open a level in the editor and build your gameplay with normal solid blocks.
2. Press **Deco** next to the undo/redo buttons.
3. Turn layers on or off and set a **density %** for each one (the chance per eligible spot).
4. Press **Generate**.

Other options:
- **Only decorate selected blocks**: decorates just the blocks you have selected, so you can work one section at a time.
- **Glow col / Deco col**: the color channel for generated objects. `0` keeps each object's default color.
- **Air height**: how many blocks above your highest block that air particles can go.
- **Clear**: deletes everything Auto Deco placed. Every generated object goes into one group (default **9999**, which you can change in the mod settings), so Clear never touches your own objects.

### Palettes: choose which objects get placed
Each layer has a list of object IDs it picks from at random. To set a list:
1. Place the decoration objects you want in the editor and select them.
2. Press **Grab** on the layer's row. The list is replaced with the IDs of your selection.

You can also type the IDs into the mod's settings (Geode menu → Auto Deco → settings).

> **About the default palettes:** the default IDs are only a starting point and may not be the exact objects you want. As a safety check, the mod **only ever places decoration-type objects**. Any ID in a palette that turns out to be a block, hazard, portal or trigger is skipped and counted in the result message, so a wrong ID can't break your gameplay. Using **Grab** is the most reliable way to set up palettes.

## Tips
- Gameplay objects (spikes, pads, orbs and so on) count as "occupied" space, so no decoration is ever stacked on top of them.
- If a big level makes the editor lag, lower the densities or the **Max objects per generation** setting.
- Generation doesn't go into the Ctrl+Z history. Use **Clear** to undo it, then change the seed or densities and generate again.

## Files
```
mod.json                     Geode metadata + settings
CMakeLists.txt               Build config
src/main.cpp                 Hooks EditorUI, adds the Deco button
src/DecoPopup.cpp/.hpp       Generator window
src/Generator.cpp/.hpp       Placement algorithm
.github/workflows/build.yml  Cloud build for Windows, macOS, Android
```
