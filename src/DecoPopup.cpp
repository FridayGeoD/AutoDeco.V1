#include "DecoPopup.hpp"

#include <cstdlib>
#include <random>

using namespace geode::prelude;
using namespace autodeco;

namespace {

constexpr float WIDTH = 420.f;
constexpr float HEIGHT = 320.f;

struct RowInfo {
    const char* name;
    const char* setting;
};

const RowInfo ROWS[5] = {
    { "Edge glow",       "glow-ids" },
    { "Ground deco",     "top-ids"  },
    { "Hanging deco",    "hang-ids" },
    { "Background deco", "bg-ids"   },
    { "Air particles",   "air-ids"  },
};

int parseInt(std::string const& s, int fallback, int lo, int hi) {
    if (s.empty()) return fallback;
    char* end = nullptr;
    long v = std::strtol(s.c_str(), &end, 10);
    if (end == s.c_str()) return fallback;
    if (v < lo) v = lo;
    if (v > hi) v = hi;
    return static_cast<int>(v);
}

}

DecoPopup* DecoPopup::create(EditorUI* ui) {
    auto ret = new DecoPopup();
    if (ret->init(ui)) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool DecoPopup::init(EditorUI* ui) {
    if (!this->initWithColor({0, 0, 0, 105})) return false;
    m_ui = ui;

    auto winSize = CCDirector::get()->getWinSize();

    m_mainLayer = CCLayer::create();
    this->addChild(m_mainLayer);

    auto bg = CCScale9Sprite::create("GJ_square01.png", {0.f, 0.f, 80.f, 80.f});
    bg->setContentSize({WIDTH, HEIGHT});
    bg->setPosition(winSize / 2);
    m_mainLayer->addChild(bg);

    m_buttonMenu = CCMenu::create();
    m_buttonMenu->setPosition(winSize / 2);
    m_buttonMenu->setZOrder(10);
    m_mainLayer->addChild(m_buttonMenu);

    auto opts = loadOptions();

    // Title + close
    auto title = CCLabelBMFont::create("Auto Deco", "goldFont.fnt");
    title->setScale(0.8f);
    title->setPosition(winSize / 2 + CCPoint{0.f, HEIGHT / 2 - 20.f});
    m_mainLayer->addChild(title, 11);

    auto closeSpr = CCSprite::createWithSpriteFrameName("GJ_closeBtn_001.png");
    closeSpr->setScale(0.8f);
    auto closeBtn = CCMenuItemSpriteExtra::create(closeSpr, this, menu_selector(DecoPopup::onClose));
    closeBtn->setPosition({-WIDTH / 2 + 5.f, HEIGHT / 2 - 5.f});
    m_buttonMenu->addChild(closeBtn);

    // Column headers
    float y = HEIGHT / 2 - 48.f;
    addLabel({-190.f, y}, "Layer", 0.35f);
    addLabel({40.f, y}, "Density %", 0.35f, {0.5f, 0.5f});
    addLabel({145.f, y}, "Palette", 0.35f, {0.5f, 0.5f});

    const bool enabled[5] = { opts.glow, opts.top, opts.hang, opts.background, opts.air };
    const int density[5] = { opts.glowDensity, opts.topDensity, opts.hangDensity, opts.bgDensity, opts.airDensity };

    for (int i = 0; i < 5; ++i) {
        y -= 25.f;
        m_rows[i].toggle = addToggle({-185.f, y}, enabled[i]);
        addLabel({-165.f, y}, ROWS[i].name, 0.4f);
        m_rows[i].density = addInput({40.f, y}, 60.f, std::to_string(density[i]));

        auto grabSpr = ButtonSprite::create("Grab", "goldFont.fnt", "GJ_button_04.png", 0.8f);
        grabSpr->setScale(0.55f);
        auto grab = CCMenuItemSpriteExtra::create(grabSpr, this, menu_selector(DecoPopup::onGrab));
        grab->setTag(i);
        grab->setPosition({145.f, y});
        m_buttonMenu->addChild(grab);
    }

    // Seed / air height
    y -= 35.f;
    addLabel({-195.f, y}, "Seed", 0.4f);
    m_seed = addInput({-110.f, y}, 90.f, std::to_string(opts.seed));
    m_seed->setFilter("0123456789");

    auto diceSpr = ButtonSprite::create("Rnd", "goldFont.fnt", "GJ_button_02.png", 0.8f);
    diceSpr->setScale(0.55f);
    auto dice = CCMenuItemSpriteExtra::create(diceSpr, this, menu_selector(DecoPopup::onRandomSeed));
    dice->setPosition({-40.f, y});
    m_buttonMenu->addChild(dice);

    addLabel({15.f, y}, "Air height", 0.4f);
    m_airHeight = addInput({140.f, y}, 50.f, std::to_string(opts.airHeight));

    // Colors
    y -= 28.f;
    addLabel({-195.f, y}, "Glow col", 0.4f);
    m_glowColor = addInput({-110.f, y}, 60.f, std::to_string(opts.glowColor));
    addLabel({15.f, y}, "Deco col", 0.4f);
    m_decoColor = addInput({140.f, y}, 50.f, std::to_string(opts.decoColor));

    // Selection only
    y -= 26.f;
    m_selectionOnly = addToggle({-185.f, y}, opts.selectionOnly);
    addLabel({-165.f, y}, "Only decorate selected blocks", 0.4f);

    // Action buttons
    auto genSpr = ButtonSprite::create("Generate", "goldFont.fnt", "GJ_button_01.png", 0.8f);
    auto gen = CCMenuItemSpriteExtra::create(genSpr, this, menu_selector(DecoPopup::onGenerate));
    gen->setPosition({-60.f, -HEIGHT / 2 + 25.f});
    m_buttonMenu->addChild(gen);

    auto clrSpr = ButtonSprite::create("Clear", "goldFont.fnt", "GJ_button_06.png", 0.8f);
    auto clr = CCMenuItemSpriteExtra::create(clrSpr, this, menu_selector(DecoPopup::onClear));
    clr->setPosition({75.f, -HEIGHT / 2 + 25.f});
    m_buttonMenu->addChild(clr);

    this->setTouchEnabled(true);
    this->setKeypadEnabled(true);
    handleTouchPriority(this);
    return true;
}

TextInput* DecoPopup::addInput(CCPoint pos, float width, std::string const& value) {
    auto input = TextInput::create(width, "0");
    input->setFilter("0123456789");
    input->setString(value);
    input->setScale(0.7f);
    input->setPosition(CCDirector::get()->getWinSize() / 2 + pos);
    m_mainLayer->addChild(input, 11);
    return input;
}

CCMenuItemToggler* DecoPopup::addToggle(CCPoint pos, bool on) {
    auto toggle = CCMenuItemToggler::createWithStandardSprites(this, menu_selector(DecoPopup::onToggle), 0.6f);
    toggle->toggle(on);
    toggle->setPosition(pos);
    m_buttonMenu->addChild(toggle);
    return toggle;
}

void DecoPopup::addLabel(CCPoint pos, std::string const& text, float scale, CCPoint anchor) {
    auto label = CCLabelBMFont::create(text.c_str(), "bigFont.fnt");
    label->setScale(scale);
    label->setAnchorPoint(anchor);
    label->setPosition(CCDirector::get()->getWinSize() / 2 + pos);
    m_mainLayer->addChild(label, 11);
}

GenOptions DecoPopup::readOptions() {
    GenOptions o;
    o.glow       = m_rows[0].toggle->isToggled();
    o.top        = m_rows[1].toggle->isToggled();
    o.hang       = m_rows[2].toggle->isToggled();
    o.background = m_rows[3].toggle->isToggled();
    o.air        = m_rows[4].toggle->isToggled();

    o.glowDensity = parseInt(m_rows[0].density->getString(), 100, 0, 100);
    o.topDensity  = parseInt(m_rows[1].density->getString(), 45, 0, 100);
    o.hangDensity = parseInt(m_rows[2].density->getString(), 35, 0, 100);
    o.bgDensity   = parseInt(m_rows[3].density->getString(), 20, 0, 100);
    o.airDensity  = parseInt(m_rows[4].density->getString(), 4, 0, 100);

    o.airHeight = parseInt(m_airHeight->getString(), 8, 0, 100);
    o.glowColor = parseInt(m_glowColor->getString(), 0, 0, 1101);
    o.decoColor = parseInt(m_decoColor->getString(), 0, 0, 1101);
    o.seed = static_cast<unsigned>(parseInt(m_seed->getString(), 1337, 0, 999999999));
    o.selectionOnly = m_selectionOnly->isToggled();
    return o;
}

void DecoPopup::saveOptions(GenOptions const& o) {
    auto m = Mod::get();
    m->setSavedValue<bool>("o-glow", o.glow);
    m->setSavedValue<bool>("o-top", o.top);
    m->setSavedValue<bool>("o-hang", o.hang);
    m->setSavedValue<bool>("o-bg", o.background);
    m->setSavedValue<bool>("o-air", o.air);
    m->setSavedValue<bool>("o-sel", o.selectionOnly);
    m->setSavedValue<int>("o-glow-d", o.glowDensity);
    m->setSavedValue<int>("o-top-d", o.topDensity);
    m->setSavedValue<int>("o-hang-d", o.hangDensity);
    m->setSavedValue<int>("o-bg-d", o.bgDensity);
    m->setSavedValue<int>("o-air-d", o.airDensity);
    m->setSavedValue<int>("o-air-h", o.airHeight);
    m->setSavedValue<int>("o-glow-c", o.glowColor);
    m->setSavedValue<int>("o-deco-c", o.decoColor);
    m->setSavedValue<int>("o-seed", static_cast<int>(o.seed));
}

GenOptions DecoPopup::loadOptions() {
    GenOptions d;
    auto m = Mod::get();
    d.glow          = m->getSavedValue<bool>("o-glow", d.glow);
    d.top           = m->getSavedValue<bool>("o-top", d.top);
    d.hang          = m->getSavedValue<bool>("o-hang", d.hang);
    d.background    = m->getSavedValue<bool>("o-bg", d.background);
    d.air           = m->getSavedValue<bool>("o-air", d.air);
    d.selectionOnly = m->getSavedValue<bool>("o-sel", d.selectionOnly);
    d.glowDensity   = m->getSavedValue<int>("o-glow-d", d.glowDensity);
    d.topDensity    = m->getSavedValue<int>("o-top-d", d.topDensity);
    d.hangDensity   = m->getSavedValue<int>("o-hang-d", d.hangDensity);
    d.bgDensity     = m->getSavedValue<int>("o-bg-d", d.bgDensity);
    d.airDensity    = m->getSavedValue<int>("o-air-d", d.airDensity);
    d.airHeight     = m->getSavedValue<int>("o-air-h", d.airHeight);
    d.glowColor     = m->getSavedValue<int>("o-glow-c", d.glowColor);
    d.decoColor     = m->getSavedValue<int>("o-deco-c", d.decoColor);
    d.seed          = static_cast<unsigned>(m->getSavedValue<int>("o-seed", static_cast<int>(d.seed)));
    return d;
}

void DecoPopup::onClose(CCObject*) {
    this->setKeypadEnabled(false);
    this->setTouchEnabled(false);
    this->removeFromParentAndCleanup(true);
}

void DecoPopup::keyBackClicked() {
    this->onClose(nullptr);
}

void DecoPopup::onGenerate(CCObject*) {
    auto opts = readOptions();
    saveOptions(opts);

    auto res = generate(m_ui, opts);

    if (res.placed == 0) {
        Notification::create(
            opts.selectionOnly ? "No solid blocks selected" : "Nothing to decorate (no solid blocks or empty palettes)",
            NotificationIcon::Warning
        )->show();
        return;
    }

    std::string msg = fmt::format("Placed {} deco objects", res.placed);
    if (res.hitCap) msg += " (hit max-object cap)";
    if (res.skippedIds) msg += fmt::format(", skipped {} non-deco ID(s)", res.skippedIds);
    Notification::create(msg, NotificationIcon::Success)->show();
    this->onClose(nullptr);
}

void DecoPopup::onClear(CCObject*) {
    auto ui = m_ui;
    geode::createQuickPopup(
        "Clear generated",
        "Remove <cy>every object</c> in the Auto Deco group?\nThis can't be undone with Ctrl+Z.",
        "Cancel", "Clear",
        [ui](FLAlertLayer*, bool confirm) {
            if (!confirm) return;
            int n = clearGenerated(ui);
            Notification::create(fmt::format("Removed {} generated objects", n), NotificationIcon::Info)->show();
        }
    );
}

void DecoPopup::onGrab(CCObject* sender) {
    int row = sender->getTag();
    if (row < 0 || row > 4) return;
    int n = grabSelectionInto(m_ui, ROWS[row].setting);
    if (n == 0) {
        Notification::create("Select some decoration objects first", NotificationIcon::Warning)->show();
        return;
    }
    Notification::create(fmt::format("{} palette set from {} object ID(s)", ROWS[row].name, n), NotificationIcon::Success)->show();
}

void DecoPopup::onRandomSeed(CCObject*) {
    std::random_device rd;
    m_seed->setString(std::to_string(rd() % 1000000000u));
}
