#include <Geode/loader/Setting.hpp>
#include <Geode/modify/CCScheduler.hpp>
#include <Geode/modify/GameManager.hpp>
#include <Geode/modify/MenuLayer.hpp>

#include <imgui-cocos.hpp>
#include <modules/config/config.hpp>
#include <modules/hack/hack.hpp>
#include <modules/keybinds/manager.hpp>

#include <modules/gui/float-button.hpp>
#include <modules/gui/blur/blur.hpp>

#include <AdvancedLabel.hpp>
#include <modules/gui/cocos/cocos.hpp>
#include <modules/gui/theming/manager.hpp>

#include <modules/i18n/DownloadPopup.hpp>
#include <modules/i18n/translations.hpp>

#include <modules/gui/components/button.hpp>
#include <modules/gui/components/color.hpp>
#include <modules/gui/components/combo.hpp>
#include <modules/gui/components/input-float.hpp>
#include <modules/gui/components/input-text.hpp>
#include <modules/gui/components/label.hpp>
#include <modules/gui/components/toggle.hpp>
#include <modules/gui/imgui/animation/easing.hpp>

using namespace geode::prelude;
using namespace superhackmenu;

static bool s_isInitialized = false;

static void toggleMenu(keybinds::KeyEvent evt = {}) {
    gui::Engine::get().toggle();
    config::save();
    gui::ThemeManager::get()->saveTheme();
}

class $modify(ClearCacheGMHook, GameManager) {
    void reloadAllStep5() {
        GameManager::reloadAllStep5();
        utils::purgeAllSingletons();
        gui::blur::cleanup();
        #ifdef SUPERHACK_USE_FLOATING_BUTTON
        gui::FloatingButton::get()->reloadSprite();
        #endif
    }
};

class $modify(SuperHackButtonMLHook, MenuLayer) {
    bool init() override {
        if (!MenuLayer::init()) return false;
        if (s_isInitialized) return true;

        gui::blur::init();
        gui::Engine::get().init();

        #ifdef SUPERHACK_USE_FLOATING_BUTTON
        gui::FloatingButton::get()->setCallback([]{ toggleMenu(); });
        #endif

        auto& key = keybinds::Manager::get()->registerKeybind("menu.toggle", "Toggle UI", toggleMenu);
        config::setIfEmpty<keybinds::KeybindProps>("menu.toggleKey", keybinds::Keys::Tab);
        key.setKey(config::get<keybinds::KeybindProps>("menu.toggleKey", keybinds::Keys::Tab));
        key.setInitialized(true);

        hack::lateInitializeHacks();
        s_isInitialized = true;
        return true;
    }
};

class HackUpdater : public cocos2d::CCObject {
public:
    static HackUpdater* create() {
        auto ret = new HackUpdater();
        ret->autorelease();
        return ret;
    }

    void update(float dt) override {
        for (const auto& hack : hack::getUpdatedHacks())
            hack->update();

        keybinds::Manager::get()->update();
        gui::blur::update(dt);
    }
};

$on_mod(Loaded) {
    auto* mod = geode::Mod::get();
    ImGuiCocos::get().setForceLegacy(mod->getSettingValue<bool>("legacy-render"));
    geode::listenForSettingChanges<bool>("legacy-render", [](bool value) {
        ImGuiCocos::get().setForceLegacy(value);
    });

    config::load();
    i18n::init();
    hack::initializeHacks();
    keybinds::Manager::get()->init();
    gui::ThemeManager::get();

    // Add bitmap fonts to texture search path
    auto bmfontPath = geode::Mod::get()->getConfigDir() / "bmfonts";
    std::error_code ec;
    std::filesystem::create_directories(bmfontPath / GEODE_MOD_ID, ec);
    if (ec) {
        geode::log::warn("Failed to create bitmap fonts directory {}: {}", bmfontPath, ec.message());
    }
    utils::get<cocos2d::CCFileUtils>()->addSearchPath(geode::utils::string::pathToString(bmfontPath).c_str());
    geode::log::info("Added bitmap fonts to search path: {}", bmfontPath);

    // Schedule hack updates
    cocos2d::CCScheduler::get()->scheduleSelector(
        schedule_selector(HackUpdater::update),
        HackUpdater::create(), 0.f, false
    );

    geode::listenForSettingChanges<std::string>("menu-style", [](std::string const& style) {
        gui::Engine::get().setRenderer(
            style == "ImGui" ? gui::RendererType::ImGui : gui::RendererType::Cocos2d
        );
    });
}
