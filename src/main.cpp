#include <Geode/Geode.hpp>
#include <Geode/modify/EditorUI.hpp>

#include "DecoPopup.hpp"

using namespace geode::prelude;

class $modify(AutoDecoEditorUI, EditorUI) {
    bool init(LevelEditorLayer* editorLayer) {
        if (!EditorUI::init(editorLayer)) return false;

        auto spr = ButtonSprite::create("Deco", "goldFont.fnt", "GJ_button_01.png", 0.8f);
        spr->setScale(0.6f);
        auto btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(AutoDecoEditorUI::onAutoDeco));
        btn->setID("auto-deco-button"_spr);

        // Put the button next to undo/redo if that menu exists, otherwise in our own menu.
        if (auto menu = this->getChildByID("undo-menu")) {
            menu->addChild(btn);
            menu->updateLayout();
        } else {
            auto winSize = CCDirector::get()->getWinSize();
            auto newMenu = CCMenu::create();
            newMenu->setID("auto-deco-menu"_spr);
            newMenu->setPosition({winSize.width / 2 - 120.f, winSize.height - 25.f});
            newMenu->addChild(btn);
            this->addChild(newMenu, 10);
        }
        return true;
    }

    void onAutoDeco(CCObject*) {
        if (auto popup = DecoPopup::create(this)) popup->show();
    }
};
