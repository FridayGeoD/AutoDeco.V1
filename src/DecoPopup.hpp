#pragma once

#include <Geode/Geode.hpp>
#include "Generator.hpp"

// The generator window. Built on FLAlertLayer directly (rather than geode::Popup)
// so it stays compatible across Geode SDK API changes.
class DecoPopup : public FLAlertLayer {
public:
    static DecoPopup* create(EditorUI* ui);

protected:
    EditorUI* m_ui = nullptr;
    cocos2d::CCLabelBMFont* m_status = nullptr;
    cocos2d::CCLabelBMFont* m_recipe = nullptr;
    cocos2d::CCLayerColor* m_underline = nullptr;
    std::vector<CCMenuItemSpriteExtra*> m_presetButtons;

    CCMenuItemToggler* m_surfaces = nullptr;
    CCMenuItemToggler* m_fill = nullptr;
    CCMenuItemToggler* m_air = nullptr;
    CCMenuItemToggler* m_mirror = nullptr;
    CCMenuItemToggler* m_skipDecorated = nullptr;
    CCMenuItemToggler* m_selectionOnly = nullptr;
    CCMenuItemToggler* m_sameBlock = nullptr;

    geode::TextInput* m_density = nullptr;
    geode::TextInput* m_airDensity = nullptr;
    geode::TextInput* m_seed = nullptr;
    geode::TextInput* m_airHeight = nullptr;

    bool init(EditorUI* ui);
    void keyBackClicked() override;

    void onClose(cocos2d::CCObject*);
    void onLearn(cocos2d::CCObject*);
    void onForget(cocos2d::CCObject*);
    void onGenerate(cocos2d::CCObject*);
    void onClear(cocos2d::CCObject*);
    void onRandomSeed(cocos2d::CCObject*);
    void onToggle(cocos2d::CCObject*) {}
    void onPreset(cocos2d::CCObject* sender);

    void updateStatus();

    geode::TextInput* addInput(cocos2d::CCPoint pos, float width, std::string const& value);
    CCMenuItemToggler* addToggle(cocos2d::CCPoint pos, bool on, std::string const& text);
    cocos2d::CCLabelBMFont* addLabel(cocos2d::CCPoint pos, std::string const& text, float scale, cocos2d::CCPoint anchor = {0.f, 0.5f});
    CCMenuItemSpriteExtra* addButton(cocos2d::CCPoint pos, const char* text, const char* bg, float scale, cocos2d::SEL_MenuHandler cb);

    autodeco::GenOptions readOptions();
    void saveOptions(autodeco::GenOptions const& opts);
    static autodeco::GenOptions loadOptions();
};
