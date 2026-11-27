#include "Generator.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <map>
#include <unordered_set>
#include <utility>

using namespace geode::prelude;

namespace autodeco {

namespace {

constexpr float CELL = 30.f;
// In the editor the ground line is at y = 90, so the first row of blocks is centered at y = 105.
constexpr int MIN_ROW = 3;
// How far (from a block's center) decoration still counts as belonging to that block.
constexpr float ATTACH_RANGE = CELL * 2.f + 1.f;
// Decoration this close to a block's center counts as "on the block" rather than on a side.
constexpr float CORE_RADIUS = 7.5f;
constexpr int MAX_STYLE_ITEMS = 4000;

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

// Deterministic per-cell randomness: same seed = same result.
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

struct Grid {
    std::unordered_set<CellKey> solid;
    std::unordered_set<CellKey> hazard;
    std::unordered_set<CellKey> occupied;
    std::vector<std::pair<int, int>> solidCells;

    bool isFree(int gx, int gy) const {
        return gy >= MIN_ROW && !occupied.count(key(gx, gy));
    }
    bool isSolid(int gx, int gy) const {
        return solid.count(key(gx, gy)) > 0;
    }
};

Grid buildGrid(LevelEditorLayer* layer, int group) {
    Grid g;
    for (auto obj : CCArrayExt<GameObject*>(layer->m_objects)) {
        if (!obj || obj->belongsToGroup(group)) continue;
        auto type = obj->m_objectType;
        if (type == GameObjectType::Decoration) continue;
        auto p = obj->getPosition();
        int gx = toCell(p.x), gy = toCell(p.y);
        CellKey k = key(gx, gy);
        if (type == GameObjectType::Solid) {
            g.occupied.insert(k);
            if (g.solid.insert(k).second) g.solidCells.push_back({gx, gy});
        }
        else if (type == GameObjectType::Hazard) {
            g.occupied.insert(k);
            g.hazard.insert(k);
        }
    }
    std::sort(g.solidCells.begin(), g.solidCells.end());
    return g;
}

struct Exposure {
    bool side[4] = { false, false, false, false };
    bool any = false;
};

Exposure exposure(Grid const& g, int gx, int gy) {
    Exposure e;
    for (int s = 0; s < 4; ++s) {
        e.side[s] = g.isFree(gx + DIR_X[s], gy + DIR_Y[s]);
        e.any = e.any || e.side[s];
    }
    return e;
}

struct Attr {
    bool air = true;
    int gx = 0, gy = 0;
    int bucket = 0;
};

// Works out which block (and which side of it) a decoration object belongs to.
Attr attribute(Grid const& g, CCPoint p, std::unordered_set<CellKey> const* only) {
    Attr a;
    int cx = toCell(p.x), cy = toCell(p.y);
    float best = ATTACH_RANGE * ATTACH_RANGE;
    bool found = false;
    for (int dx = -2; dx <= 2; ++dx) {
        for (int dy = -2; dy <= 2; ++dy) {
            int gx = cx + dx, gy = cy + dy;
            CellKey k = key(gx, gy);
            bool ok = only ? only->count(k) > 0 : g.solid.count(k) > 0;
            if (!ok) continue;
            float ox = p.x - cellCenter(gx), oy = p.y - cellCenter(gy);
            float d = ox * ox + oy * oy;
            if (d < best) {
                best = d;
                a.gx = gx;
                a.gy = gy;
                found = true;
            }
        }
    }
    if (!found) return a;

    a.air = false;
    float ox = p.x - cellCenter(a.gx), oy = p.y - cellCenter(a.gy);
    auto e = exposure(g, a.gx, a.gy);
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
    a.bucket = bucket;
    return a;
}

Item makeItem(GameObject* obj, float cx, float cy) {
    Item it;
    auto p = obj->getPosition();
    it.id = obj->m_objectID;
    it.dx = p.x - cx;
    it.dy = p.y - cy;
    it.rot = obj->getRotation();
    it.sx = std::fabs(obj->getScaleX());
    it.sy = std::fabs(obj->getScaleY());
    it.fx = obj->isFlipX();
    it.fy = obj->isFlipY();
    it.col = obj->m_baseColor ? obj->m_baseColor->m_colorID : 0;
    it.dcol = obj->m_detailColor ? obj->m_detailColor->m_colorID : 0;
    it.zl = static_cast<int>(obj->m_zLayer);
    return it;
}

class Placer {
public:
    explicit Placer(LevelEditorLayer* layer) : m_layer(layer) {
        m_cap = static_cast<int>(Mod::get()->getSettingValue<int64_t>("max-objects"));
        m_group = genGroup();
    }

    bool full() const { return m_result.placed >= m_cap; }
    GenResult const& result() const { return m_result; }

    void place(Item const& it, CCPoint pos, bool mirror) {
        if (full()) {
            m_result.hitCap = true;
            return;
        }
        auto obj = m_layer->createObject(it.id, pos, true);
        if (!obj) return;

        float rot = mirror ? -it.rot : it.rot;
        if (rot != 0.f) obj->setRotation(rot);
        if (std::fabs(it.sx - 1.f) > 0.001f && it.sx > 0.f) obj->updateCustomScaleX(it.sx);
        if (std::fabs(it.sy - 1.f) > 0.001f && it.sy > 0.f) obj->updateCustomScaleY(it.sy);
        bool fx = mirror ? !it.fx : it.fx;
        if (fx) obj->setFlipX(true);
        if (it.fy) obj->setFlipY(true);
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
        "{},{:.2f},{:.2f},{:.2f},{:.3f},{:.3f},{},{},{},{},{}",
        it.id, it.dx, it.dy, it.rot, it.sx, it.sy,
        it.fx ? 1 : 0, it.fy ? 1 : 0, it.col, it.dcol, it.zl
    );
}

bool itemFromString(std::string const& s, Item& it) {
    auto f = split(s, ',');
    if (f.size() != 11) return false;
    it.id = std::atoi(f[0].c_str());
    it.dx = std::strtof(f[1].c_str(), nullptr);
    it.dy = std::strtof(f[2].c_str(), nullptr);
    it.rot = std::strtof(f[3].c_str(), nullptr);
    it.sx = std::strtof(f[4].c_str(), nullptr);
    it.sy = std::strtof(f[5].c_str(), nullptr);
    it.fx = std::atoi(f[6].c_str()) != 0;
    it.fy = std::atoi(f[7].c_str()) != 0;
    it.col = std::atoi(f[8].c_str());
    it.dcol = std::atoi(f[9].c_str());
    it.zl = std::atoi(f[10].c_str());
    if (it.sx <= 0.f) it.sx = 1.f;
    if (it.sy <= 0.f) it.sy = 1.f;
    return it.id > 0;
}

void saveStyle() {
    Mod::get()->setSavedValue<std::string>("style", currentStyle().serialize());
}

}

// ---------- Style ----------

bool Style::empty() const {
    for (int b = 0; b < BUCKETS; ++b) {
        if (!patterns[b].empty()) return false;
    }
    return air.empty();
}

int Style::patternCount() const {
    int n = 0;
    for (int b = 0; b < BUCKETS; ++b) n += static_cast<int>(patterns[b].size());
    return n;
}

int Style::itemCount() const {
    int n = static_cast<int>(air.size());
    for (int b = 0; b < BUCKETS; ++b) {
        for (auto const& p : patterns[b]) n += static_cast<int>(p.size());
    }
    return n;
}

std::string Style::serialize() const {
    std::string out = "ADS1";
    for (int b = 0; b < BUCKETS; ++b) {
        out += fmt::format("\n{} {}", b, slots[b]);
        for (auto const& pat : patterns[b]) {
            out += ' ';
            for (size_t i = 0; i < pat.size(); ++i) {
                if (i) out += ';';
                out += itemToString(pat[i]);
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
    if (lines.empty() || lines[0] != "ADS1") return st;

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

        int b = std::atoi(tok[0].c_str());
        if (b < 0 || b >= BUCKETS) continue;
        st.slots[b] = std::atoi(tok[1].c_str());
        for (size_t t = 2; t < tok.size(); ++t) {
            Pattern pat;
            for (auto const& part : split(tok[t], ';')) {
                Item it;
                if (itemFromString(part, it)) pat.push_back(it);
            }
            if (!pat.empty()) st.patterns[b].push_back(std::move(pat));
        }
    }
    return st;
}

Style& currentStyle() {
    static Style style;
    static bool loaded = false;
    if (!loaded) {
        loaded = true;
        style = Style::deserialize(Mod::get()->getSavedValue<std::string>("style", ""));
    }
    return style;
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

    std::unordered_set<CellKey> sample;
    std::vector<std::pair<int, int>> sampleCells;
    std::vector<GameObject*> decos;

    for (auto obj : CCArrayExt<GameObject*>(sel)) {
        if (!obj) continue;
        if (obj->m_objectType == GameObjectType::Solid) {
            auto p = obj->getPosition();
            int gx = toCell(p.x), gy = toCell(p.y);
            if (sample.insert(key(gx, gy)).second) sampleCells.push_back({gx, gy});
        }
        else if (obj->m_objectType == GameObjectType::Decoration) {
            decos.push_back(obj);
        }
    }

    res.solids = static_cast<int>(sampleCells.size());
    res.decos = static_cast<int>(decos.size());
    if (sampleCells.empty() || decos.empty()) return res;

    Style st;

    // Count every kind of spot in the sample, so we know how often the user decorated each kind.
    int minX = sampleCells.front().first, maxX = minX;
    int minY = sampleCells.front().second, maxY = minY;
    for (auto [gx, gy] : sampleCells) {
        auto e = exposure(grid, gx, gy);
        for (int s = 0; s < 4; ++s) {
            if (e.side[s]) st.slots[s]++;
        }
        st.slots[e.any ? CORE_SURFACE : CORE_INTERIOR]++;
        minX = std::min(minX, gx); maxX = std::max(maxX, gx);
        minY = std::min(minY, gy); maxY = std::max(maxY, gy);
    }

    // Group the decoration by (block, side).
    std::map<std::pair<CellKey, int>, Pattern> groups;
    int stored = 0;
    for (auto obj : decos) {
        if (stored >= MAX_STYLE_ITEMS) break;
        auto p = obj->getPosition();
        auto a = attribute(grid, p, &sample);
        if (a.air) {
            int gx = toCell(p.x), gy = toCell(p.y);
            st.air.push_back(makeItem(obj, cellCenter(gx), cellCenter(gy)));
            minX = std::min(minX, gx); maxX = std::max(maxX, gx);
            minY = std::min(minY, gy); maxY = std::max(maxY, gy);
        }
        else {
            groups[{ key(a.gx, a.gy), a.bucket }].push_back(makeItem(obj, cellCenter(a.gx), cellCenter(a.gy)));
        }
        stored++;
    }
    for (auto& [k, pat] : groups) {
        st.patterns[k.second].push_back(std::move(pat));
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

    // Which blocks to decorate.
    std::vector<std::pair<int, int>> targets;
    if (opts.selectionOnly) {
        std::unordered_set<CellKey> seen;
        if (auto sel = ui->getSelectedObjects()) {
            for (auto obj : CCArrayExt<GameObject*>(sel)) {
                if (!obj || obj->m_objectType != GameObjectType::Solid || obj->belongsToGroup(group)) continue;
                auto p = obj->getPosition();
                int gx = toCell(p.x), gy = toCell(p.y);
                if (seen.insert(key(gx, gy)).second) targets.push_back({gx, gy});
            }
        }
        std::sort(targets.begin(), targets.end());
    }
    else {
        targets = grid.solidCells;
    }
    if (targets.empty()) return placer.result();

    // Spots the user already decorated by hand get left alone.
    std::unordered_set<CellKey> decorated[BUCKETS];
    if (opts.skipDecorated) {
        for (auto obj : CCArrayExt<GameObject*>(layer->m_objects)) {
            if (!obj || obj->m_objectType != GameObjectType::Decoration || obj->belongsToGroup(group)) continue;
            auto a = attribute(grid, obj->getPosition(), nullptr);
            if (!a.air) decorated[a.bucket].insert(key(a.gx, a.gy));
        }
    }

    float density = std::max(0, opts.density) / 100.f;

    auto stamp = [&](Pattern const& pat, int gx, int gy, bool mirror) {
        float cx = cellCenter(gx), cy = cellCenter(gy);
        for (auto const& it : pat) {
            float dx = mirror ? -it.dx : it.dx;
            CCPoint pos { cx + dx, cy + it.dy };
            int tx = toCell(pos.x), ty = toCell(pos.y);
            if (grid.hazard.count(key(tx, ty))) continue;
            bool outside = std::fabs(it.dx) > CELL / 2.f || std::fabs(it.dy) > CELL / 2.f;
            if (outside && grid.isSolid(tx, ty)) continue;
            placer.place(it, pos, mirror);
            if (placer.full()) return;
        }
    };

    auto tryBucket = [&](int bucket, int gx, int gy, int salt) {
        auto const& pats = st.patterns[bucket];
        if (pats.empty() || st.slots[bucket] <= 0) return;
        if (decorated[bucket].count(key(gx, gy))) return;
        Rng rng(opts.seed, gx, gy, salt);
        float p = std::min(1.f, static_cast<float>(pats.size()) / static_cast<float>(st.slots[bucket])) * density;
        if (!rng.chance(p)) return;
        auto const& pat = pats[rng.index(pats.size())];
        bool canMirror = bucket == TOP || bucket == BOTTOM || bucket == CORE_SURFACE || bucket == CORE_INTERIOR;
        bool mirror = opts.mirror && canMirror && rng.chance(0.5f);
        stamp(pat, gx, gy, mirror);
    };

    for (auto [gx, gy] : targets) {
        if (placer.full()) break;
        auto e = exposure(grid, gx, gy);
        if (opts.fill) {
            tryBucket(e.any ? CORE_SURFACE : CORE_INTERIOR, gx, gy, 100);
        }
        if (opts.surfaces) {
            for (int s = 0; s < 4; ++s) {
                if (e.side[s]) tryBucket(s, gx, gy, 10 + s);
            }
        }
    }

    // Air particles over the decorated area.
    if (opts.air && !st.air.empty() && st.airCells > 0 && !placer.full()) {
        int minX = targets.front().first, maxX = targets.back().first;
        int maxY = MIN_ROW;
        for (auto const& t : targets) maxY = std::max(maxY, t.second);
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
                float dx = mirror ? -it.dx : it.dx;
                placer.place(it, { cellCenter(gx) + dx, cellCenter(gy) + it.dy }, mirror);
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
