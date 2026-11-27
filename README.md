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

## Block matching (v2.2)
- Decoration is stored **relative to the exact block it sits on**, including that block's position, rotation and flip. Off-grid blocks, half blocks and nudged blocks all line up correctly.
- When generating, each pattern is **rotated and flipped to match the target block**. For example, if a block is rotated 90°, its "top" deco ends up on its right side, turned 90°.
- Decoration is matched to the **block type** (object ID) it was learned on. **Same block only** (on by default) decorates only block types that were in your sample. Turn it off to let every block type use your patterns.
- v2.2 saves styles in a new format, so learn each preset again once.

## Style presets

The window has five style slots: **Heliopolis**, **TS II** (Thinking Space II), **Grief**, **Acheron** and **Custom**. Tap one to switch to it. **Learn** saves your sample into the selected slot, and **Generate** uses the selected slot. Each slot is saved separately, so build each sample once and reuse it in every level.

Make each sample look like its reference. These are the color channels and objects to use:

### Heliopolis (green)
- **Colors:** background `#0B1A0B`, main glow / outlines `#46FF46`, detail `#1E4A1E`, silhouettes `#3A4A3A`.
- **Block sides:** 2–3 offset thin line outlines along each edge to get the stacked-brick look, plus a bright green glow piece on top.
- **Block fill:** small brick or line detail pieces inside blocks.
- **Below ceilings:** hanging lamps on a bent pole, and a chain now and then.
- **Air:** small green and yellow square pixels and particle bits.
- **Background (B layers):** darker staircase and pillar silhouettes, plus zigzag spike deco on the edges.

### Thinking Space II (black and white)
- **Colors:** background `#050505`, outlines `#FFFFFF`, detail `#7A7A7A`, accent `#3DFF6E` (use it rarely).
- **Block sides:** thin white outline lines with a soft white glow.
- **Block fill:** gray square or panel detail.
- **Below ceilings:** hanging light bulbs and lamps.
- **Air:** lots of crystal and glass shards at different rotations, plus a few small green bits. Set **Air %** higher, around 150.

### Grief (red)
- **Colors:** background `#3A0000`, glow `#FF2020`, shapes `#000000`, mid red `#8B0000`.
- **Block sides:** black shapes with a strong red glow on the edges, and red zigzag spike deco.
- **Block fill:** black, with an occasional red triangle detail.
- **Below ceilings:** big black chains.
- **Background (B layers):** large dark-red saw blades and spiky shapes.

### Acheron (red and black)
- **Colors:** background `#0B0B0D`, blocks `#1C1C1F`, outlines `#E01E1E`, highlights `#FFFFFF`.
- **Block sides:** thin red outline lines, plus gray spike deco on tops.
- **Block fill:** dark panels, with a red "eye" face detail on some blocks.
- **Below ceilings:** hanging chains with lamps.
- **Background:** dark saw blades.

Set the color channels with the editor's normal color menu. The mod copies the channel numbers each object uses, so generated deco takes on the same colors.

## Build
Push this folder to a GitHub repo. The included workflow builds the Windows `.geode` file. Download **Build Output** from the run's Summary page, then put the `.geode` file in `geode/mods`.

Targets **Geode 6.0.0 / GD 2.2091**.
