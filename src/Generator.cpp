#include "Generator.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

using namespace geode::prelude;

namespace autodeco {

namespace {

constexpr float CELL = 30.f;
// In the editor the ground line is at y = 90, so the first row of blocks is centered at y = 105.
constexpr int MIN_ROW = 3;

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

// Deterministic per-cell random numbers so the same seed always gives the same result.
uint64_t splitmix(uint64_t x) {
    x += 0x9E3779B97F4A7C15ull;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
    return x ^ (x >> 31);
}

struct Rng {
    uint64_t state;
    Rng(unsigned seed, int gx, int gy, int salt) {
        state = splitmix(seed ^ splitmix(static_cast<uint64_t>(gx) * 73856093ull ^
                                          static_cast<uint64_t>(gy) * 19349663ull ^
                                          static_cast<uint64_t>(salt) * 83492791ull));
    }
    float next() { // [0, 1)
        state = splitmix(state);
        return static_cast<float>(state >> 40) / static_cast<float>(1ull << 24);
    }
    float range(float a, float b) { return a + (b - a) * next(); }
    bool chance(int percent) { return next() * 100.f < static_cast<float>(percent); }
    template <class T>
    T const& pick(std::vector<T> const& v) {
        return v[static_cast<size_t>(next() * v.size()) % v.size()];
    }
};

enum Salt { SALT_GLOW = 1, SALT_TOP, SALT_HANG, SALT_BG, SALT_AIR };

class Placer {
public:
    Placer(LevelEditorLayer* layer, GenOptions const& opts)
        : m_layer(layer), m_opts(opts) {
        m_cap = static_cast<int>(Mod::get()->getSettingValue<int64_t>("max-objects"));
        m_group = static_cast<int>(Mod::get()->getSettingValue<int64_t>("gen-group"));
    }

    bool full() const { return m_result.placed >= m_cap; }

    // Places one decoration object. Returns nullptr if the ID isn't a decoration
    // (so a wrong ID in settings can never drop a portal or spike into your level).
    GameObject* place(int id, CCPoint pos, float rotation, float scale, bool flip, int color, bool background) {
        if (full()) {
            m_result.hitCap = true;
            return nullptr;
        }
        auto known = m_valid.find(id);
        if (known != m_valid.end() && !known->second) return nullptr;

        auto obj = m_layer->createObject(id, pos, true);
        if (!obj) {
            m_valid[id] = false;
            m_result.skippedIds++;
            return nullptr;
        }
        if (known == m_valid.end()) {
            bool ok = obj->m_objectType == GameObjectType::Decoration;
            m_valid[id] = ok;
            if (!ok) {
                log::warn("Auto Deco: object ID {} is not a decoration object, skipping it", id);
                m_layer->removeObject(obj, true);
                m_result.skippedIds++;
                return nullptr;
            }
        }

        if (rotation != 0.f) obj->setRotation(rotation);
        if (scale != 1.f) {
            obj->updateCustomScaleX(scale);
            obj->updateCustomScaleY(scale);
        }
        if (flip) obj->setFlipX(true);
        if (color > 0 && obj->m_baseColor) obj->m_baseColor->m_colorID = color;
        if (background) obj->m_zLayer = ZLayer::B2;
        obj->addToGroup(m_group);

        m_result.placed++;
        return obj;
    }

    int group() const { return m_group; }
    GenResult const& result() const { return m_result; }

private:
    LevelEditorLayer* m_layer;
    GenOptions const& m_opts;
    std::unordered_map<int, bool> m_valid;
    GenResult m_result;
    int m_cap = 5000;
    int m_group = 9999;
};

bool isSolid(GameObject* obj) {
    return obj->m_objectType == GameObjectType::Solid;
}

}

std::vector<int> parseIds(std::string const& str) {
    std::vector<int> out;
    std::stringstream ss(str);
    std::string part;
    while (std::getline(ss, part, ',')) {
        part.erase(std::remove_if(part.begin(), part.end(), [](char c) { return std::isspace(static_cast<unsigned char>(c)); }), part.end());
        if (part.empty()) continue;
        if (!std::all_of(part.begin(), part.end(), [](char c) { return std::isdigit(static_cast<unsigned char>(c)); })) continue;
        if (part.size() > 5) continue;
        int id = std::stoi(part);
        if (id > 0 && std::find(out.begin(), out.end(), id) == out.end()) out.push_back(id);
    }
    return out;
}

GenResult generate(EditorUI* ui, GenOptions const& opts) {
    auto layer = ui->m_editorLayer;
    Placer placer(layer, opts);
    int group = placer.group();

    // 1. Build an occupancy grid of everything that isn't decoration, and collect solid cells.
    std::unordered_set<CellKey> occupied;
    std::vector<std::pair<int, int>> solids;
    std::unordered_set<CellKey> solidSet;

    auto addSolid = [&](GameObject* obj) {
        auto p = obj->getPosition();
        int gx = toCell(p.x), gy = toCell(p.y);
        if (solidSet.insert(key(gx, gy)).second) solids.push_back({gx, gy});
    };

    for (auto obj : CCArrayExt<GameObject*>(layer->m_objects)) {
        if (!obj || obj->belongsToGroup(group)) continue;
        if (obj->m_objectType == GameObjectType::Decoration) continue;
        auto p = obj->getPosition();
        occupied.insert(key(toCell(p.x), toCell(p.y)));
        if (!opts.selectionOnly && isSolid(obj)) addSolid(obj);
    }

    if (opts.selectionOnly) {
        if (auto sel = ui->getSelectedObjects()) {
            for (auto obj : CCArrayExt<GameObject*>(sel)) {
                if (obj && isSolid(obj) && !obj->belongsToGroup(group)) addSolid(obj);
            }
        }
    }

    if (solids.empty()) return placer.result();

    std::sort(solids.begin(), solids.end());

    auto isFree = [&](int gx, int gy) {
        return gy >= MIN_ROW && !occupied.count(key(gx, gy));
    };

    auto glowIds = parseIds(Mod::get()->getSettingValue<std::string>("glow-ids"));
    auto topIds  = parseIds(Mod::get()->getSettingValue<std::string>("top-ids"));
    auto hangIds = parseIds(Mod::get()->getSettingValue<std::string>("hang-ids"));
    auto bgIds   = parseIds(Mod::get()->getSettingValue<std::string>("bg-ids"));
    auto airIds  = parseIds(Mod::get()->getSettingValue<std::string>("air-ids"));

    // Cells we've already decorated, so passes don't stack objects on each other.
    std::unordered_set<CellKey> decorated;

    // 2. Glow along every exposed edge.
    if (opts.glow && !glowIds.empty()) {
        struct Dir { int dx, dy; float rot; };
        const Dir dirs[] = { {0, 1, 0.f}, {1, 0, 90.f}, {0, -1, 180.f}, {-1, 0, 270.f} };
        for (auto [gx, gy] : solids) {
            for (auto const& d : dirs) {
                int nx = gx + d.dx, ny = gy + d.dy;
                if (!isFree(nx, ny)) continue;
                Rng rng(opts.seed, gx * 4 + d.dx + 2, gy * 4 + d.dy + 2, SALT_GLOW);
                if (!rng.chance(opts.glowDensity)) continue;
                placer.place(rng.pick(glowIds), {cellCenter(nx), cellCenter(ny)}, d.rot, 1.f, false, opts.glowColor, false);
                if (placer.full()) break;
            }
        }
    }

    // 3. Ground deco on top surfaces.
    if (opts.top && !topIds.empty() && !placer.full()) {
        for (auto [gx, gy] : solids) {
            if (!isFree(gx, gy + 1)) continue;
            Rng rng(opts.seed, gx, gy, SALT_TOP);
            if (!rng.chance(opts.topDensity)) continue;
            CCPoint pos { cellCenter(gx) + rng.range(-6.f, 6.f), cellCenter(gy + 1) };
            if (placer.place(rng.pick(topIds), pos, 0.f, rng.range(0.8f, 1.1f), rng.chance(50), opts.decoColor, false)) {
                decorated.insert(key(gx, gy + 1));
            }
            if (placer.full()) break;
        }
    }

    // 4. Hanging deco under ceilings (not on the ground row).
    if (opts.hang && !hangIds.empty() && !placer.full()) {
        for (auto [gx, gy] : solids) {
            if (gy <= MIN_ROW || !isFree(gx, gy - 1)) continue;
            if (decorated.count(key(gx, gy - 1))) continue;
            Rng rng(opts.seed, gx, gy, SALT_HANG);
            if (!rng.chance(opts.hangDensity)) continue;
            CCPoint pos { cellCenter(gx) + rng.range(-4.f, 4.f), cellCenter(gy - 1) };
            if (placer.place(rng.pick(hangIds), pos, 0.f, rng.range(0.85f, 1.1f), rng.chance(50), opts.decoColor, false)) {
                decorated.insert(key(gx, gy - 1));
            }
            if (placer.full()) break;
        }
    }

    // 5. Background deco behind blocks.
    if (opts.background && !bgIds.empty() && !placer.full()) {
        for (auto [gx, gy] : solids) {
            Rng rng(opts.seed, gx, gy, SALT_BG);
            if (!rng.chance(opts.bgDensity)) continue;
            CCPoint pos { cellCenter(gx) + rng.range(-15.f, 15.f), cellCenter(gy) + rng.range(-15.f, 15.f) };
            float rot = std::floor(rng.range(0.f, 4.f)) * 90.f;
            placer.place(rng.pick(bgIds), pos, rot, rng.range(0.8f, 1.6f), rng.chance(50), opts.decoColor, true);
            if (placer.full()) break;
        }
    }

    // 6. Air particles in empty space over the level's footprint.
    if (opts.air && !airIds.empty() && !placer.full()) {
        int minX = solids.front().first, maxX = solids.back().first;
        int maxY = MIN_ROW;
        for (auto const& s : solids) maxY = std::max(maxY, s.second);
        maxY += std::max(0, opts.airHeight);

        for (int gx = minX; gx <= maxX && !placer.full(); ++gx) {
            for (int gy = MIN_ROW; gy <= maxY; ++gy) {
                if (!isFree(gx, gy) || decorated.count(key(gx, gy))) continue;
                Rng rng(opts.seed, gx, gy, SALT_AIR);
                if (!rng.chance(opts.airDensity)) continue;
                CCPoint pos { cellCenter(gx) + rng.range(-12.f, 12.f), cellCenter(gy) + rng.range(-12.f, 12.f) };
                placer.place(rng.pick(airIds), pos, rng.range(0.f, 360.f), rng.range(0.4f, 1.0f), false, opts.decoColor, false);
                if (placer.full()) break;
            }
        }
    }

    return placer.result();
}

int clearGenerated(EditorUI* ui) {
    auto layer = ui->m_editorLayer;
    int group = static_cast<int>(Mod::get()->getSettingValue<int64_t>("gen-group"));

    std::vector<Ref<GameObject>> toRemove;
    for (auto obj : CCArrayExt<GameObject*>(layer->m_objects)) {
        if (obj && obj->belongsToGroup(group)) toRemove.push_back(obj);
    }
    if (toRemove.empty()) return 0;

    ui->deselectAll();
    for (auto& obj : toRemove) layer->removeObject(obj, true);
    return static_cast<int>(toRemove.size());
}

int grabSelectionInto(EditorUI* ui, std::string const& settingKey) {
    auto sel = ui->getSelectedObjects();
    if (!sel || sel->count() == 0) return 0;

    std::vector<int> ids;
    for (auto obj : CCArrayExt<GameObject*>(sel)) {
        if (!obj) continue;
        int id = obj->m_objectID;
        if (std::find(ids.begin(), ids.end(), id) == ids.end()) ids.push_back(id);
    }

    std::string out;
    for (size_t i = 0; i < ids.size(); ++i) {
        if (i) out += ",";
        out += std::to_string(ids[i]);
    }
    Mod::get()->setSettingValue<std::string>(settingKey, out);
    return static_cast<int>(ids.size());
}

}
