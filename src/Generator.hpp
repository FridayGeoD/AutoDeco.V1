#pragma once

#include <Geode/Geode.hpp>
#include <vector>
#include <string>

namespace autodeco {

struct GenOptions {
    bool glow = true;
    bool top = true;
    bool hang = true;
    bool background = true;
    bool air = true;
    bool selectionOnly = false;

    // Chance per eligible cell, 0-100
    int glowDensity = 100;
    int topDensity = 45;
    int hangDensity = 35;
    int bgDensity = 20;
    int airDensity = 4;

    // How many cells above the highest block air particles may spawn
    int airHeight = 8;

    // 0 = leave the object's default color
    int glowColor = 0;
    int decoColor = 0;

    unsigned int seed = 1337;
};

struct GenResult {
    int placed = 0;
    int skippedIds = 0;
    bool hitCap = false;
};

// Parses "1,2, 3" into {1,2,3}; ignores junk.
std::vector<int> parseIds(std::string const& str);

// Generates decoration in the given editor.
GenResult generate(EditorUI* ui, GenOptions const& opts);

// Removes every object in the generated group. Returns how many were removed.
int clearGenerated(EditorUI* ui);

// Writes the unique object IDs of the current selection into a palette setting.
// Returns the number of IDs grabbed.
int grabSelectionInto(EditorUI* ui, std::string const& settingKey);

}
