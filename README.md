# Auto Deco v2: Geode mod for Geometry Dash

Decorate a few blocks by hand, and Auto Deco **learns your style** and decorates the rest of the level with it.

It records every decoration object on your sample blocks, including its object, position, rotation, scale, flip, colors and Z layer. It also records where each object sits on its block:

| Spot | Examples |
|---|---|
| **Top / bottom / left / right side** | Glow, grass, outlines, hanging chains |
| **Block fill** | Detail pieces placed on the blocks themselves (outer blocks and buried inner blocks are learned separately) |
| **Air** | Particles and floating deco away from blocks |

When you press Generate, it applies those patterns to every matching spot in the level. It uses about the same density as your sample, randomly picks between your different patterns, and can mirror them for variety.

## How to use

1. **Make a sample.** Pick a section with 5–15 blocks and decorate it by hand the way you want the whole level to look. Include tops, sides, bottoms and some air particles. More variety in the sample gives more variety in the result.
2. **Select the sample.** Box-select those blocks **and** all the decoration on them.
3. Press **Deco**, then **Learn from selection**. The window shows how many patterns it learned.
4. Press **Generate**.

The learned style is saved, so you can reuse it in other levels.

### Options
- **Block sides / Block fill / Air particles**: turn each part on or off.
- **Mirror variety**: randomly mirrors patterns so repeats are less obvious.
- **Skip hand-deco**: leaves spots you already decorated by hand alone, including your sample.
- **Only selected**: decorates only the blocks you have selected.
- **Density %**: 100 matches your sample's density. Lower it for a cleaner look.
- **Air %**: the same idea for air particles, and it can go above 100.
- **Seed / Rnd**: the same seed gives the same result. Roll a new one for a different variation.
- **Clear**: removes everything Auto Deco placed (it all goes into group 9999, which you can change in the mod settings). Your own decoration is kept.

## Build
Push this folder to a GitHub repo. The included workflow builds the Windows `.geode` file. Download **Build Output** from the run's Summary page, then put the `.geode` file in `geode/mods`.

Targets **Geode 6.0.0 / GD 2.2091**.
