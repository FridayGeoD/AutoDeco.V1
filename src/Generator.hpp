#pragma once

#include <Geode/Geode.hpp>
#include <string>
#include <vector>

namespace autodeco {

// One decoration object, stored relative to the block it belongs to.
struct Item {
    int id = 0;
    float dx = 0.f, dy = 0.f;
    float rot = 0.f;
    float sx = 1.f, sy = 1.f;
    bool fx = false, fy = false;
    int col = 0, dcol = 0;
    int zl = 0;
};

// Everything that was placed on one spot of one block (for example "the top of block X").
using Pattern = std::vector<Item>;

// Where on a block a pattern goes.
enum Bucket {
    TOP = 0,
    RIGHT = 1,
    BOTTOM = 2,
    LEFT = 3,
    CORE_SURFACE = 4,  // on/inside a block that touches open space
    CORE_INTERIOR = 5, // on/inside a block buried inside other blocks
    BUCKETS = 6
};

struct Style {
    std::vector<Pattern> patterns[BUCKETS];
    int slots[BUCKETS] = {};   // how many spots of each kind existed in the sample
    std::vector<Item> air;     // particles etc. floating away from blocks
    int airCells = 0;          // free cells in the sample area (for air density)

    bool empty() const;
    int patternCount() const;
    int itemCount() const;
    std::string serialize() const;
    static Style deserialize(std::string const& data);
};

struct GenOptions {
    bool surfaces = true;
    bool fill = true;
    bool air = true;
    bool mirror = true;
    bool skipDecorated = true;
    bool selectionOnly = false;
    int density = 100;     // % of the sample's density
    int airDensity = 100;  // % of the sample's air density
    int airHeight = 8;     // cells above the highest block
    unsigned int seed = 1337;
};

struct LearnResult {
    int solids = 0;
    int decos = 0;
    int patterns = 0;
};

struct GenResult {
    int placed = 0;
    bool hitCap = false;
};

Style& currentStyle();
void forgetStyle();

LearnResult learnFromSelection(EditorUI* ui);
GenResult generate(EditorUI* ui, GenOptions const& opts);
int clearGenerated(EditorUI* ui);

}
