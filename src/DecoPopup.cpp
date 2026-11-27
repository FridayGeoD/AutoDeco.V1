#include "DecoPopup.hpp"

#include <cstdlib>
#include <random>

using namespace geode::prelude;
using namespace autodeco;

namespace {

constexpr float WIDTH = 420.f;
constexpr float HEIGHT = 320.f;
const char* PRESET_LABELS[autodeco::SLOT_COUNT] = { "Heliopolis", "TS II", "Grief", "Acheron", "Custom" };

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

    // Style presets
    for (int i = 0; i < SLOT_COUNT; ++i) {
        auto btn = addButton({-168.f + 84.f * i, 112.f}, PRESET_LABELS[i], "GJ_button_04.png", 0.5f, menu_selector(DecoPopup::onPreset));
        btn->setTag(i);
        m_presetButtons.push_back(btn);
    }
    m_underline = CCLayerColor::create({255, 220, 60, 255}, 64.f, 3.f);
    m_mainLayer->addChild(m_underline, 11);

    m_status = addLabel({0.f, 84.f}, "", 0.45f, {0.5f, 0.5f});

    addButton({-60.f, 60.f}, "Learn from selection", "GJ_button_01.png", 0.6f, menu_selector(DecoPopup::onLearn));
    addButton({110.f, 60.f}, "Forget", "GJ_button_06.png", 0.6f, menu_selector(DecoPopup::onForget));

    m_recipe = addLabel({0.f, 38.f}, "", 0.29f, {0.5f, 0.5f});
    m_recipe->setColor({255, 230, 140});
    auto hint = addLabel({0.f, 24.f}, "Deco a few blocks, select them + their deco, press Learn", 0.3f, {0.5f, 0.5f});
    hint->setColor({200, 200, 255});
    updateStatus();

    // Toggles
    m_surfaces      = addToggle({-190.f, 4.f}, opts.surfaces, "Block sides");
    m_fill          = addToggle({-190.f, -17.f}, opts.fill, "Block fill");
    m_air           = addToggle({-190.f, -38.f}, opts.air, "Air particles");
    m_sameBlock     = addToggle({-190.f, -59.f}, opts.sameBlockOnly, "Same block only");
    m_mirror        = addToggle({15.f, 4.f}, opts.mirror, "Mirror variety");
    m_skipDecorated = addToggle({15.f, -17.f}, opts.skipDecorated, "Skip hand-deco");
    m_selectionOnly = addToggle({15.f, -38.f}, opts.selectionOnly, "Only selected");

    // Numbers
    addLabel({-200.f, -84.f}, "Density %", 0.4f);
    m_density = addInput({-80.f, -84.f}, 60.f, std::to_string(opts.density));
    addLabel({15.f, -84.f}, "Air %", 0.4f);
    m_airDensity = addInput({140.f, -84.f}, 60.f, std::to_string(opts.airDensity));

    addLabel({-200.f, -110.f}, "Seed", 0.4f);
    m_seed = addInput({-110.f, -110.f}, 90.f, std::to_string(opts.seed));
    addButton({-38.f, -110.f}, "Rnd", "GJ_button_02.png", 0.55f, menu_selector(DecoPopup::onRandomSeed));
    addLabel({15.f, -110.f}, "Air height", 0.4f);
    m_airHeight = addInput({140.f, -110.f}, 60.f, std::to_string(opts.airHeight));

    // Actions
    addButton({-60.f, -HEIGHT / 2 + 20.f}, "Generate", "GJ_button_01.png", 0.8f, menu_selector(DecoPopup::onGenerate));
    addButton({75.f, -HEIGHT / 2 + 20.f}, "Clear", "GJ_button_06.png", 0.8f, menu_selector(DecoPopup::onClear));

    this->setTouchEnabled(true);
    this->setKeypadEnabled(true);
    handleTouchPriority(this);
    return true;
}

void DecoPopup::updateStatus() {
    int slot = activeSlot();
    auto const& st = currentStyle();
    if (st.empty()) {
        m_status->setString(fmt::format("{}: not learned yet", slotName(slot)).c_str());
        m_status->setColor({255, 160, 160});
    }
    else {
        m_status->setString(fmt::format("{}: {} patterns, {} objects", slotName(slot), st.patternCount(), st.itemCount()).c_str());
        m_status->setColor({160, 255, 160});
    }
    m_recipe->setString(slotRecipe(slot));

    if (m_underline && slot < static_cast<int>(m_presetButtons.size())) {
        auto pos = m_presetButtons[slot]->getPosition();
        m_underline->setPosition(CCDirector::get()->getWinSize() / 2 + CCPoint{pos.x - 32.f, pos.y - 16.f});
    }
}

void DecoPopup::onPreset(CCObject* sender) {
    setActiveSlot(sender->getTag());
    updateStatus();
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

CCMenuItemToggler* DecoPopup::addToggle(CCPoint pos, bool on, std::string const& text) {
    auto toggle = CCMenuItemToggler::createWithStandardSprites(this, menu_selector(DecoPopup::onToggle), 0.55f);
    toggle->toggle(on);
    toggle->setPosition(pos);
    m_buttonMenu->addChild(toggle);
    addLabel({pos.x + 18.f, pos.y}, text, 0.4f);
    return toggle;
}

CCLabelBMFont* DecoPopup::addLabel(CCPoint pos, std::string const& text, float scale, CCPoint anchor) {
    auto label = CCLabelBMFont::create(text.c_str(), "bigFont.fnt");
    label->setScale(scale);
    label->setAnchorPoint(anchor);
    label->setPosition(CCDirector::get()->getWinSize() / 2 + pos);
    m_mainLayer->addChild(label, 11);
    return label;
}

CCMenuItemSpriteExtra* DecoPopup::addButton(CCPoint pos, const char* text, const char* bg, float scale, SEL_MenuHandler cb) {
    auto spr = ButtonSprite::create(text, "goldFont.fnt", bg, 0.8f);
    spr->setScale(scale);
    auto btn = CCMenuItemSpriteExtra::create(spr, this, cb);
    btn->setPosition(pos);
    m_buttonMenu->addChild(btn);
    return btn;
}

GenOptions DecoPopup::readOptions() {
    GenOptions o;
    o.surfaces = m_surfaces->isToggled();
    o.fill = m_fill->isToggled();
    o.air = m_air->isToggled();
    o.mirror = m_mirror->isToggled();
    o.skipDecorated = m_skipDecorated->isToggled();
    o.selectionOnly = m_selectionOnly->isToggled();
    o.sameBlockOnly = m_sameBlock->isToggled();
    o.density = parseInt(m_density->getString(), 100, 0, 100);
    o.airDensity = parseInt(m_airDensity->getString(), 100, 0, 1000);
    o.airHeight = parseInt(m_airHeight->getString(), 8, 0, 100);
    o.seed = static_cast<unsigned>(parseInt(m_seed->getString(), 1337, 0, 999999999));
    return o;
}

void DecoPopup::saveOptions(GenOptions const& o) {
    auto m = Mod::get();
    m->setSavedValue<bool>("o2-surfaces", o.surfaces);
    m->setSavedValue<bool>("o2-fill", o.fill);
    m->setSavedValue<bool>("o2-air", o.air);
    m->setSavedValue<bool>("o2-mirror", o.mirror);
    m->setSavedValue<bool>("o2-skip", o.skipDecorated);
    m->setSavedValue<bool>("o2-sel", o.selectionOnly);
    m->setSavedValue<bool>("o2-same-block", o.sameBlockOnly);
    m->setSavedValue<int>("o2-density", o.density);
    m->setSavedValue<int>("o2-air-density", o.airDensity);
    m->setSavedValue<int>("o2-air-height", o.airHeight);
    m->setSavedValue<int>("o2-seed", static_cast<int>(o.seed));
}

GenOptions DecoPopup::loadOptions() {
    GenOptions d;
    auto m = Mod::get();
    d.surfaces      = m->getSavedValue<bool>("o2-surfaces", d.surfaces);
    d.fill          = m->getSavedValue<bool>("o2-fill", d.fill);
    d.air           = m->getSavedValue<bool>("o2-air", d.air);
    d.mirror        = m->getSavedValue<bool>("o2-mirror", d.mirror);
    d.skipDecorated = m->getSavedValue<bool>("o2-skip", d.skipDecorated);
    d.selectionOnly = m->getSavedValue<bool>("o2-sel", d.selectionOnly);
    d.sameBlockOnly = m->getSavedValue<bool>("o2-same-block", d.sameBlockOnly);
    d.density       = m->getSavedValue<int>("o2-density", d.density);
    d.airDensity    = m->getSavedValue<int>("o2-air-density", d.airDensity);
    d.airHeight     = m->getSavedValue<int>("o2-air-height", d.airHeight);
    d.seed          = static_cast<unsigned>(m->getSavedValue<int>("o2-seed", static_cast<int>(d.seed)));
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

void DecoPopup::onLearn(CCObject*) {
    auto res = learnFromSelection(m_ui);
    if (res.solids == 0 || res.decos == 0) {
        Notification::create("Select some blocks AND the decoration you put on them", NotificationIcon::Warning)->show();
        return;
    }
    updateStatus();
    Notification::create(
        fmt::format("{}: learned {} patterns for {} block type(s) from {} deco objects", slotName(activeSlot()), res.patterns, res.blockTypes, res.decos),
        NotificationIcon::Success
    )->show();
}

void DecoPopup::onForget(CCObject*) {
    forgetStyle();
    updateStatus();
    Notification::create(fmt::format("{} style forgotten", slotName(activeSlot())), NotificationIcon::Info)->show();
}

void DecoPopup::onGenerate(CCObject*) {
    if (currentStyle().empty()) {
        Notification::create(fmt::format("Learn the {} style first: decorate a few blocks, select them, press Learn", slotName(activeSlot())), NotificationIcon::Warning)->show();
        return;
    }

    auto opts = readOptions();
    saveOptions(opts);

    auto res = generate(m_ui, opts);
    if (res.placed == 0) {
        std::string why = "Nothing new to decorate";
        if (opts.selectionOnly) why = "No solid blocks selected";
        else if (res.skippedBlocks > 0) why = "None of your blocks match the sample's block types. Turn off 'Same block only' or learn with these blocks";
        Notification::create(why, NotificationIcon::Warning)->show();
        return;
    }

    std::string msg = fmt::format("Placed {} deco objects", res.placed);
    if (res.hitCap) msg += " (hit max-object cap)";
    if (res.skippedBlocks > 0) msg += fmt::format(", skipped {} blocks of other types", res.skippedBlocks);
    Notification::create(msg, NotificationIcon::Success)->show();
    this->onClose(nullptr);
}

void DecoPopup::onClear(CCObject*) {
    auto ui = m_ui;
    geode::createQuickPopup(
        "Clear generated",
        "Remove <cy>every object</c> Auto Deco placed?\nYour hand-made deco is kept. This can't be undone with Ctrl+Z.",
        "Cancel", "Clear",
        [ui](FLAlertLayer*, bool confirm) {
            if (!confirm) return;
            int n = clearGenerated(ui);
            Notification::create(fmt::format("Removed {} generated objects", n), NotificationIcon::Info)->show();
        }
    );
}

void DecoPopup::onRandomSeed(CCObject*) {
    std::random_device rd;
    m_seed->setString(std::to_string(rd() % 1000000000u));
}
