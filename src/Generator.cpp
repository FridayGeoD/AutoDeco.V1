#include "Generator.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <unordered_map>
#include <unordered_set>
#include <utility>

using namespace geode::prelude;

namespace autodeco {

namespace {

constexpr float PI = 3.14159265358979f;
constexpr float CELL = 30.f;
// In the editor the ground line is at y = 90, so the first row of blocks is centered at y = 105.
constexpr int MIN_ROW = 3;
// How far (from a block's position) decoration still counts as belonging to that block.
constexpr float ATTACH_RANGE = CELL * 2.f + 1.f;
// Decoration this close to a block's center counts as "on the block" rather than on a side.
constexpr float CORE_RADIUS = 7.5f;
constexpr int MAX_STYLE_ITEMS = 4000;

// Sides in clockwise order: top, right, bottom, left.
const int DIR_X[4] = { 0, 1, 0, -1 };
const int DIR_Y[4] = { 1, 0, -1, 0 };

using CellKey = int64_t;

CellKey key(int gx, int gy) {
    return (static_cast<int64_t>(gx) << 32) ^ static_cast<uint32_t>(gy);
}

int toCell(float v) {
    return static_cast<int>(std::floor(v / CELL));
}

float cellCenter(int g) {
    return g * CELL + CELL / 2.f;
}

uint64_t splitmix(uint64_t x) {
    x += 0x9E3779B97F4A7C15ull;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
    return x ^ (x >> 31);
}

// Deterministic per-block randomness: same seed = same result.
struct Rng {
    uint64_t state;
    Rng(unsigned seed, int gx, int gy, int salt) {
        state = splitmix(seed ^ splitmix(static_cast<uint64_t>(gx) * 73856093ull ^
                                          static_cast<uint64_t>(gy) * 19349663ull ^
                                          static_cast<uint64_t>(salt) * 83492791ull));
    }
    float next() {
        state = splitmix(state);
        return static_cast<float>(state >> 40) / static_cast<float>(1ull << 24);
    }
    bool chance(float p) { return next() < p; }
    size_t index(size_t n) { return static_cast<size_t>(next() * n) % n; }
};

int genGroup() {
    return static_cast<int>(Mod::get()->getSettingValue<int64_t>("gen-group"));
}

// ---------- block transforms ----------

// A block's transform: decoration is stored relative to this.
struct Anchor {
    CCPoint pos;
    float rot = 0.f; // clockwise degrees (cocos convention)
    bool fx = false;
    int k = 0;       // rotation in quarter turns, for mapping sides
    int id = 0;
};

Anchor anchorOf(GameObject* b) {
    Anchor a;
    a.pos = b->getPosition();
    a.rot = b->getRotation();
    a.fx = b->isFlipX();
    // A vertical flip is the same as a horizontal flip plus a half turn.
    if (b->isFlipY()) {
        a.fx = !a.fx;
        a.rot += 180.f;
    }
    a.k = ((static_cast<int>(std::lround(a.rot / 90.f)) % 4) + 4) % 4;
    a.id = b->m_objectID;
    return a;
}

// Rotates a vector clockwise by `deg` degrees.
CCPoint rotVec(CCPoint v, float deg) {
    float r = deg * PI / 180.f;
    float c = std::cos(r), s = std::sin(r);
    return { v.x * c + v.y * s, -v.x * s + v.y * c };
}

CCPoint toLocal(Anchor const& a, CCPoint world) {
    CCPoint v = rotVec({ world.x - a.pos.x, world.y - a.pos.y }, -a.rot);
    if (a.fx) v.x = -v.x;
    return v;
}

CCPoint toWorld(Anchor const& a, CCPoint local) {
    if (a.fx) local.x = -local.x;
    CCPoint v = rotVec(local, a.rot);
    return { a.pos.x + v.x, a.pos.y + v.y };
}

int mirrorSide(int s) {
    return s == RIGHT ? LEFT : (s == LEFT ? RIGHT : s);
}

// Which of the block's own sides faces world side `s`.
int worldToLocalSide(Anchor const& a, int s) {
    int unrotated = ((s - a.k) % 4 + 4) % 4;
    return a.fx ? mirrorSide(unrotated) : unrotated;
}

// ---------- level grid ----------

struct Grid {
    std::unordered_set<CellKey> hazard;
    std::unordered_set<CellKey> occupied;
    std::unordered_map<CellKey, GameObject*> blockAt; // first solid block in each cell
    std::vector<GameObject*> blocks;                  // every solid block, in level order

    bool isFree(int gx, int gy) const {
        return gy >= MIN_ROW && !occupied.count(key(gx, gy));
    }
    bool isSolid(int gx, int gy) const {
        return blockAt.count(key(gx, gy)) > 0;
    }
};

Grid buildGrid(LevelEditorLayer* layer, int group) {
    Grid g;
    for (auto obj : CCArrayExt<GameObject*>(layer->m_objects)) {
        if (!obj || obj->belongsToGroup(group)) continue;
        auto type = obj->m_objectType;
        if (type == GameObjectType::Decoration) continue;
        auto p = obj->getPosition();
        CellKey k = key(toCell(p.x), toCell(p.y));
        if (type == GameObjectType::Solid) {
            g.occupied.insert(k);
            if (g.blockAt.emplace(k, obj).second) g.blocks.push_back(obj);
        }
        else if (type == GameObjectType::Hazard) {
            g.occupied.insert(k);
            g.hazard.insert(k);
        }
    }
    std::sort(g.blocks.begin(), g.blocks.end(), [](GameObject* a, GameObject* b) {
        auto pa = a->getPosition(), pb = b->getPosition();
        return pa.x != pb.x ? pa.x < pb.x : pa.y < pb.y;
    });
    return g;
}

struct Exposure {
    bool side[4] = { false, false, false, false };
    bool any = false;
};

Exposure exposureAt(Grid const& g, CCPoint blockPos) {
    Exposure e;
    int gx = toCell(blockPos.x), gy = toCell(blockPos.y);
    for (int s = 0; s < 4; ++s) {
        e.side[s] = g.isFree(gx + DIR_X[s], gy + DIR_Y[s]);
        e.any = e.any || e.side[s];
    }
    return e;
}

struct Attr {
    bool air = true;
    GameObject* block = nullptr;
    int worldBucket = 0;
};

// Works out which block (and which side of it, in world space) a decoration object belongs to.
Attr attribute(Grid const& g, CCPoint p, std::unordered_map<CellKey, GameObject*> const& blockMap) {
    Attr a;
    int cx = toCell(p.x), cy = toCell(p.y);
    float best = ATTACH_RANGE * ATTACH_RANGE;
    for (int dx = -2; dx <= 2; ++dx) {
        for (int dy = -2; dy <= 2; ++dy) {
            auto it = blockMap.find(key(cx + dx, cy + dy));
            if (it == blockMap.end()) continue;
            auto bp = it->second->getPosition();
            float ox = p.x - bp.x, oy = p.y - bp.y;
            float d = ox * ox + oy * oy;
            if (d < best) {
                best = d;
                a.block = it->second;
            }
        }
    }
    if (!a.block) return a;

    a.air = false;
    auto bp = a.block->getPosition();
    float ox = p.x - bp.x, oy = p.y - bp.y;
    auto e = exposureAt(g, bp);
    int bucket = e.any ? CORE_SURFACE : CORE_INTERIOR;
    if (std::fabs(ox) > CORE_RADIUS || std::fabs(oy) > CORE_RADIUS) {
        float bestDot = 0.f;
        for (int s = 0; s < 4; ++s) {
            if (!e.side[s]) continue;
            float dot = ox * DIR_X[s] + oy * DIR_Y[s];
            if (dot > bestDot) {
                bestDot = dot;
                bucket = s;
            }
        }
    }
    a.worldBucket = bucket;
    return a;
}

// Stores a decoration object relative to a block (or to a plain cell for air items).
Item makeItem(GameObject* obj, Anchor const& a) {
    Item it;
    it.id = obj->m_objectID;

    float rot = obj->getRotation();
    bool fx = obj->isFlipX();
    if (obj->isFlipY()) {
        fx = !fx;
        rot += 180.f;
    }

    auto local = toLocal(a, obj->getPosition());
    it.dx = local.x;
    it.dy = local.y;
    it.rot = a.fx ? -(rot - a.rot) : (rot - a.rot);
    it.fx = fx != a.fx;

    it.sx = std::fabs(obj->getScaleX());
    it.sy = std::fabs(obj->getScaleY());
    it.col = obj->m_baseColor ? obj->m_baseColor->m_colorID : 0;
    it.dcol = obj->m_detailColor ? obj->m_detailColor->m_colorID : 0;
    it.zl = static_cast<int>(obj->m_zLayer);
    return it;
}

Anchor cellAnchor(int gx, int gy) {
    Anchor a;
    a.pos = { cellCenter(gx), cellCenter(gy) };
    return a;
}

class Placer {
public:
    explicit Placer(LevelEditorLayer* layer) : m_layer(layer) {
        m_cap = static_cast<int>(Mod::get()->getSettingValue<int64_t>("max-objects"));
        m_group = genGroup();
    }

    bool full() const { return m_result.placed >= m_cap; }
    GenResult& result() { return m_result; }

    // Places an item on a block, optionally mirrored in the block's own frame.
    void place(Item const& it, Anchor const& a, bool mirror, Grid const& grid) {
        if (full()) {
            m_result.hitCap = true;
            return;
        }

        float lx = mirror ? -it.dx : it.dx;
        float lrot = mirror ? -it.rot : it.rot;
        bool lfx = mirror ? !it.fx : it.fx;

        CCPoint pos = toWorld(a, { lx, it.dy });
        int tx = toCell(pos.x), ty = toCell(pos.y);
        if (grid.hazard.count(key(tx, ty))) return;
        bool outside = std::fabs(it.dx) > CELL / 2.f || std::fabs(it.dy) > CELL / 2.f;
        if (outside && grid.isSolid(tx, ty)) return;

        float rot = a.rot + (a.fx ? -lrot : lrot);
        bool fx = a.fx != lfx;

        auto obj = m_layer->createObject(it.id, pos, true);
        if (!obj) return;

        rot = std::fmod(rot, 360.f);
        if (rot > 180.f) rot -= 360.f;
        if (rot < -180.f) rot += 360.f;
        if (std::fabs(rot) > 0.01f) obj->setRotation(rot);
        if (std::fabs(it.sx - 1.f) > 0.001f && it.sx > 0.f) obj->updateCustomScaleX(it.sx);
        if (std::fabs(it.sy - 1.f) > 0.001f && it.sy > 0.f) obj->updateCustomScaleY(it.sy);
        if (fx) obj->setFlipX(true);
        if (it.col > 0 && obj->m_baseColor) obj->m_baseColor->m_colorID = it.col;
        if (it.dcol > 0 && obj->m_detailColor) obj->m_detailColor->m_colorID = it.dcol;
        obj->m_zLayer = static_cast<ZLayer>(it.zl);
        obj->addToGroup(m_group);
        m_result.placed++;
    }

private:
    LevelEditorLayer* m_layer;
    GenResult m_result;
    int m_cap = 5000;
    int m_group = 9999;
};

// ---------- saving ----------

std::vector<std::string> split(std::string const& s, char sep) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : s) {
        if (c == sep) {
            out.push_back(cur);
            cur.clear();
        }
        else {
            cur += c;
        }
    }
    out.push_back(cur);
    return out;
}

std::string itemToString(Item const& it) {
    return fmt::format(
        "{},{:.2f},{:.2f},{:.2f},{:.3f},{:.3f},{},{},{},{}",
        it.id, it.dx, it.dy, it.rot, it.sx, it.sy, it.fx ? 1 : 0, it.col, it.dcol, it.zl
    );
}

bool itemFromString(std::string const& s, Item& it) {
    auto f = split(s, ',');
    if (f.size() != 10) return false;
    it.id = std::atoi(f[0].c_str());
    it.dx = std::strtof(f[1].c_str(), nullptr);
    it.dy = std::strtof(f[2].c_str(), nullptr);
    it.rot = std::strtof(f[3].c_str(), nullptr);
    it.sx = std::strtof(f[4].c_str(), nullptr);
    it.sy = std::strtof(f[5].c_str(), nullptr);
    it.fx = std::atoi(f[6].c_str()) != 0;
    it.col = std::atoi(f[7].c_str());
    it.dcol = std::atoi(f[8].c_str());
    it.zl = std::atoi(f[9].c_str());
    if (it.sx <= 0.f) it.sx = 1.f;
    if (it.sy <= 0.f) it.sy = 1.f;
    return it.id > 0;
}

const char* SLOT_KEYS[SLOT_COUNT] = { "heliopolis", "thinking-space-2", "grief", "acheron", "custom" };
const char* SLOT_NAMES[SLOT_COUNT] = { "Heliopolis", "Thinking Space II", "Grief", "Acheron", "Custom" };
const char* SLOT_RECIPES[SLOT_COUNT] = {
    "Green brick outlines, green glow, lamps, chains, pixel bits",
    "White outlines on black, crystal shards, hanging lights",
    "Black shapes, red glow, big chains, saws in the background",
    "Dark blocks, red outlines, chains, lamps, glowing eyes",
    "Your own style",
};

int clampSlot(int slot) {
    return slot < 0 || slot >= SLOT_COUNT ? 0 : slot;
}

std::string slotKey(int slot) {
    return std::string("style-v3-") + SLOT_KEYS[clampSlot(slot)];
}

Style& styleForSlot(int slot) {
    static Style styles[SLOT_COUNT];
    static bool loaded[SLOT_COUNT] = {};
    slot = clampSlot(slot);
    if (!loaded[slot]) {
        loaded[slot] = true;
        styles[slot] = Style::deserialize(Mod::get()->getSavedValue<std::string>(slotKey(slot), ""));
    }
    return styles[slot];
}

void saveStyle() {
    Mod::get()->setSavedValue<std::string>(slotKey(activeSlot()), currentStyle().serialize());
}

}

// ---------- Style ----------

bool Style::empty() const {
    for (auto const& [id, bs] : blocks) {
        for (int b = 0; b < BUCKETS; ++b) {
            if (!bs.patterns[b].empty()) return false;
        }
    }
    return air.empty();
}

int Style::patternCount() const {
    int n = 0;
    for (auto const& [id, bs] : blocks) {
        for (int b = 0; b < BUCKETS; ++b) n += static_cast<int>(bs.patterns[b].size());
    }
    return n;
}

int Style::itemCount() const {
    int n = static_cast<int>(air.size());
    for (auto const& [id, bs] : blocks) {
        for (int b = 0; b < BUCKETS; ++b) {
            for (auto const& p : bs.patterns[b]) n += static_cast<int>(p.size());
        }
    }
    return n;
}

std::string Style::serialize() const {
    std::string out = "ADS3";
    for (auto const& [id, bs] : blocks) {
        for (int b = 0; b < BUCKETS; ++b) {
            if (bs.slots[b] <= 0 && bs.patterns[b].empty()) continue;
            out += fmt::format("\nB {} {} {}", id, b, bs.slots[b]);
            for (auto const& pat : bs.patterns[b]) {
                out += ' ';
                for (size_t i = 0; i < pat.size(); ++i) {
                    if (i) out += ';';
                    out += itemToString(pat[i]);
                }
            }
        }
    }
    out += fmt::format("\nA {}", airCells);
    for (auto const& it : air) {
        out += ' ';
        out += itemToString(it);
    }
    return out;
}

Style Style::deserialize(std::string const& data) {
    Style st;
    auto lines = split(data, '\n');
    if (lines.empty() || lines[0] != "ADS3") return st;

    for (size_t i = 1; i < lines.size(); ++i) {
        auto tok = split(lines[i], ' ');
        if (tok.size() < 2) continue;

        if (tok[0] == "A") {
            st.airCells = std::atoi(tok[1].c_str());
            for (size_t t = 2; t < tok.size(); ++t) {
                Item it;
                if (itemFromString(tok[t], it)) st.air.push_back(it);
            }
            continue;
        }

        if (tok[0] != "B" || tok.size() < 4) continue;
        int id = std::atoi(tok[1].c_str());
        int b = std::atoi(tok[2].c_str());
        if (id <= 0 || b < 0 || b >= BUCKETS) continue;
        auto& bs = st.blocks[id];
        bs.slots[b] = std::atoi(tok[3].c_str());
        for (size_t t = 4; t < tok.size(); ++t) {
            Pattern pat;
            for (auto const& part : split(tok[t], ';')) {
                Item it;
                if (itemFromString(part, it)) pat.push_back(it);
            }
            if (!pat.empty()) bs.patterns[b].push_back(std::move(pat));
        }
    }
    return st;
}

const char* slotName(int slot) {
    return SLOT_NAMES[clampSlot(slot)];
}

const char* slotRecipe(int slot) {
    return SLOT_RECIPES[clampSlot(slot)];
}

int activeSlot() {
    return clampSlot(Mod::get()->getSavedValue<int>("active-slot", 0));
}

void setActiveSlot(int slot) {
    Mod::get()->setSavedValue<int>("active-slot", clampSlot(slot));
}

Style& currentStyle() {
    return styleForSlot(activeSlot());
}

void forgetStyle() {
    currentStyle() = Style{};
    saveStyle();
}

// ---------- Learning ----------

LearnResult learnFromSelection(EditorUI* ui) {
    LearnResult res;
    auto sel = ui->getSelectedObjects();
    if (!sel || sel->count() == 0) return res;

    auto layer = ui->m_editorLayer;
    int group = genGroup();
    Grid grid = buildGrid(layer, group);

    std::unordered_map<CellKey, GameObject*> sample;
    std::vector<GameObject*> sampleBlocks;
    std::vector<GameObject*> decos;

    for (auto obj : CCArrayExt<GameObject*>(sel)) {
        if (!obj) continue;
        if (obj->m_objectType == GameObjectType::Solid) {
            auto p = obj->getPosition();
            if (sample.emplace(key(toCell(p.x), toCell(p.y)), obj).second) sampleBlocks.push_back(obj);
        }
        else if (obj->m_objectType == GameObjectType::Decoration) {
            decos.push_back(obj);
        }
    }

    res.solids = static_cast<int>(sampleBlocks.size());
    res.decos = static_cast<int>(decos.size());
    if (sampleBlocks.empty() || decos.empty()) return res;

    Style st;

    // Count every kind of spot in the sample, per block type, in each block's own frame.
    auto first = sampleBlocks.front()->getPosition();
    int minX = toCell(first.x), maxX = minX;
    int minY = toCell(first.y), maxY = minY;
    for (auto b : sampleBlocks) {
        auto a = anchorOf(b);
        auto e = exposureAt(grid, a.pos);
        auto& bs = st.blocks[a.id];
        for (int s = 0; s < 4; ++s) {
            if (e.side[s]) bs.slots[worldToLocalSide(a, s)]++;
        }
        bs.slots[e.any ? CORE_SURFACE : CORE_INTERIOR]++;
        int gx = toCell(a.pos.x), gy = toCell(a.pos.y);
        minX = std::min(minX, gx); maxX = std::max(maxX, gx);
        minY = std::min(minY, gy); maxY = std::max(maxY, gy);
    }

    // Group the decoration by (block, side), stored relative to its block.
    std::map<std::pair<GameObject*, int>, Pattern> groups;
    int stored = 0;
    for (auto obj : decos) {
        if (stored >= MAX_STYLE_ITEMS) break;
        auto p = obj->getPosition();
        auto at = attribute(grid, p, sample);
        if (at.air) {
            int gx = toCell(p.x), gy = toCell(p.y);
            st.air.push_back(makeItem(obj, cellAnchor(gx, gy)));
            minX = std::min(minX, gx); maxX = std::max(maxX, gx);
            minY = std::min(minY, gy); maxY = std::max(maxY, gy);
        }
        else {
            auto a = anchorOf(at.block);
            int localBucket = at.worldBucket < 4 ? worldToLocalSide(a, at.worldBucket) : at.worldBucket;
            groups[{ at.block, localBucket }].push_back(makeItem(obj, a));
        }
        stored++;
    }
    for (auto& [k, pat] : groups) {
        st.blocks[k.first->m_objectID].patterns[k.second].push_back(std::move(pat));
    }

    // Drop block types that ended up with no decoration at all.
    for (auto it = st.blocks.begin(); it != st.blocks.end();) {
        bool any = false;
        for (int b = 0; b < BUCKETS; ++b) any = any || !it->second.patterns[b].empty();
        if (any) ++it;
        else it = st.blocks.erase(it);
    }

    // How much open space the sample had, for air particle density.
    if (!st.air.empty()) {
        int freeCells = 0;
        for (int gx = minX; gx <= maxX; ++gx) {
            for (int gy = std::max(minY, MIN_ROW); gy <= maxY; ++gy) {
                if (grid.isFree(gx, gy)) freeCells++;
            }
        }
        st.airCells = std::max(1, freeCells);
    }

    res.patterns = st.patternCount() + (st.air.empty() ? 0 : 1);
    res.blockTypes = static_cast<int>(st.blocks.size());
    currentStyle() = std::move(st);
    saveStyle();
    return res;
}

// ---------- Generating ----------

GenResult generate(EditorUI* ui, GenOptions const& opts) {
    Style const& st = currentStyle();
    if (st.empty()) return {};

    auto layer = ui->m_editorLayer;
    int group = genGroup();
    Grid grid = buildGrid(layer, group);
    Placer placer(layer);

    // All block types pooled together, for blocks whose type wasn't in the sample.
    BlockStyle pooled;
    if (!opts.sameBlockOnly) {
        for (auto const& [id, bs] : st.blocks) {
            for (int b = 0; b < BUCKETS; ++b) {
                pooled.slots[b] += bs.slots[b];
                pooled.patterns[b].insert(pooled.patterns[b].end(), bs.patterns[b].begin(), bs.patterns[b].end());
            }
        }
    }

    // Which blocks to decorate.
    std::vector<GameObject*> targets;
    if (opts.selectionOnly) {
        if (auto sel = ui->getSelectedObjects()) {
            for (auto obj : CCArrayExt<GameObject*>(sel)) {
                if (obj && obj->m_objectType == GameObjectType::Solid && !obj->belongsToGroup(group)) {
                    targets.push_back(obj);
                }
            }
        }
    }
    else {
        targets = grid.blocks;
    }
    if (targets.empty()) return placer.result();

    // Spots the user already decorated by hand get left alone.
    std::unordered_set<GameObject*> decorated[BUCKETS];
    if (opts.skipDecorated) {
        for (auto obj : CCArrayExt<GameObject*>(layer->m_objects)) {
            if (!obj || obj->m_objectType != GameObjectType::Decoration || obj->belongsToGroup(group)) continue;
            auto at = attribute(grid, obj->getPosition(), grid.blockAt);
            if (!at.air) decorated[at.worldBucket].insert(at.block);
        }
    }

    float density = std::max(0, opts.density) / 100.f;

    for (auto block : targets) {
        if (placer.full()) break;

        auto a = anchorOf(block);
        BlockStyle const* bs = nullptr;
        auto found = st.blocks.find(a.id);
        if (found != st.blocks.end()) bs = &found->second;
        else if (!opts.sameBlockOnly) bs = &pooled;
        if (!bs) {
            placer.result().skippedBlocks++;
            continue;
        }

        auto e = exposureAt(grid, a.pos);
        int gx = toCell(a.pos.x), gy = toCell(a.pos.y);

        auto tryBucket = [&](int worldBucket, int localBucket, int salt) {
            auto const& pats = bs->patterns[localBucket];
            if (pats.empty() || bs->slots[localBucket] <= 0) return;
            if (decorated[worldBucket].count(block)) return;
            Rng rng(opts.seed, gx, gy, salt);
            float p = std::min(1.f, static_cast<float>(pats.size()) / static_cast<float>(bs->slots[localBucket])) * density;
            if (!rng.chance(p)) return;
            auto const& pat = pats[rng.index(pats.size())];
            // Mirroring left<->right only makes sense for patterns on the top, bottom or the block itself.
            bool canMirror = localBucket == TOP || localBucket == BOTTOM ||
                             localBucket == CORE_SURFACE || localBucket == CORE_INTERIOR;
            bool mirror = opts.mirror && canMirror && rng.chance(0.5f);
            for (auto const& it : pat) {
                placer.place(it, a, mirror, grid);
                if (placer.full()) return;
            }
        };

        if (opts.fill) {
            int core = e.any ? CORE_SURFACE : CORE_INTERIOR;
            tryBucket(core, core, 100);
        }
        if (opts.surfaces) {
            for (int s = 0; s < 4; ++s) {
                if (e.side[s]) tryBucket(s, worldToLocalSide(a, s), 10 + s);
            }
        }
    }

    // Air particles over the decorated area.
    if (opts.air && !st.air.empty() && st.airCells > 0 && !placer.full()) {
        int minX = toCell(targets.front()->getPosition().x), maxX = minX;
        int maxY = MIN_ROW;
        for (auto t : targets) {
            auto p = t->getPosition();
            minX = std::min(minX, toCell(p.x));
            maxX = std::max(maxX, toCell(p.x));
            maxY = std::max(maxY, toCell(p.y));
        }
        maxY += std::max(0, opts.airHeight);

        float p = std::min(1.f, static_cast<float>(st.air.size()) / static_cast<float>(st.airCells)) *
                  (std::max(0, opts.airDensity) / 100.f);

        for (int gx = minX; gx <= maxX && !placer.full(); ++gx) {
            for (int gy = MIN_ROW; gy <= maxY; ++gy) {
                if (!grid.isFree(gx, gy)) continue;
                Rng rng(opts.seed, gx, gy, 200);
                if (!rng.chance(p)) continue;
                auto const& it = st.air[rng.index(st.air.size())];
                bool mirror = opts.mirror && rng.chance(0.5f);
                placer.place(it, cellAnchor(gx, gy), mirror, grid);
                if (placer.full()) break;
            }
        }
    }

    return placer.result();
}

int clearGenerated(EditorUI* ui) {
    auto layer = ui->m_editorLayer;
    int group = genGroup();

    std::vector<Ref<GameObject>> toRemove;
    for (auto obj : CCArrayExt<GameObject*>(layer->m_objects)) {
        if (obj && obj->belongsToGroup(group)) toRemove.push_back(obj);
    }
    if (toRemove.empty()) return 0;

    ui->deselectAll();
    for (auto& obj : toRemove) layer->removeObject(obj, true);
    return static_cast<int>(toRemove.size());
}

}
