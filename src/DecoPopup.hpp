#pragma once

#include <Geode/Geode.hpp>
#include "Generator.hpp"

// The generator window. Built on FLAlertLayer directly (rather than geode::Popup)
// so it stays compatible across Geode SDK API changes.
class DecoPopup : public FLAlertLayer {
public:
    static DecoPopup* create(EditorUI* ui);

protected:
    struct Row {
        CCMenuItemToggler* toggle = nullptr;
        geode::TextInput* density = nullptr;
    };

    EditorUI* m_ui = nullptr;
    Row m_rows[5];
    geode::TextInput* m_seed = nullptr;
    geode::TextInput* m_airHeight = nullptr;
    geode::TextInput* m_glowColor = nullptr;
    geode::TextInput* m_decoColor = nullptr;
    CCMenuItemToggler* m_selectionOnly = nullptr;

    bool init(EditorUI* ui);
    void keyBackClicked() override;

    void onClose(cocos2d::CCObject*);
    void onGenerate(cocos2d::CCObject*);
    void onClear(cocos2d::CCObject*);
    void onGrab(cocos2d::CCObject* sender);
    void onRandomSeed(cocos2d::CCObject*);
    void onToggle(cocos2d::CCObject*) {}

    geode::TextInput* addInput(cocos2d::CCPoint pos, float width, std::string const& value);
    CCMenuItemToggler* addToggle(cocos2d::CCPoint pos, bool on);
    void addLabel(cocos2d::CCPoint pos, std::string const& text, float scale, cocos2d::CCPoint anchor = {0.f, 0.5f});

    autodeco::GenOptions readOptions();
    void saveOptions(autodeco::GenOptions const& opts);
    static autodeco::GenOptions loadOptions();
};
