#include <modules/chatbot/ui/ChatboxPopup.hpp>
#include <modules/config/config.hpp>
#include <modules/gui/gui.hpp>
#include <modules/gui/components/button.hpp>
#include <modules/gui/components/keybind.hpp>
#include <modules/hack/hack.hpp>
#include <modules/i18n/translations.hpp>

struct ToggleDevToolsEvent : geode::Event<ToggleDevToolsEvent, bool()> {
    using Event::Event;
};

namespace superhack::Shortcuts {
    class $hack(Shortcuts) {
        using FileEvent = geode::Task<geode::Result<std::filesystem::path>>;

        static void openSettings() {
            if (auto* options = OptionsLayer::create()) {
                auto scene = utils::get<cocos2d::CCScene>();
                if (!scene) return;
                auto zOrder = scene->getHighestChildZ();
                scene->addChild(options, zOrder + 1);
                options->showLayer(false);
            }
        }

        static void openGraphicSettings() {
            #ifndef GEODE_IS_MACOS
            if (auto* options = VideoOptionsLayer::create()) {
                options->show();
            }
            #endif
        }

        static std::string getLevelKey(int levelID, bool isOnline, bool isDaily, bool isGauntlet, bool isEvent) {
            if (isOnline) return fmt::format("c_{}", levelID);
            if (isDaily) return fmt::format("d_{}", levelID);
            if (isGauntlet) return fmt::format("g_{}", levelID);
            if (isEvent) return fmt::format("e_{}", levelID);
            return fmt::format("n_{}", levelID);
        }

        static void uncompleteLevel() {
            auto scene = utils::get<cocos2d::CCScene>();
            if (!scene) return;
            GJGameLevel* level = nullptr;

            if (auto* pl = utils::get<PlayLayer>()) {
                level = pl->m_level;
            } else if (auto* lil = scene->getChildByType<LevelInfoLayer>(0)) {
                level = lil->m_level;
            }

            if (!level || level->m_levelType != GJLevelType::Saved)
                return Popup::create(i18n::get_("common.error"), i18n::get_("shortcuts.uncomplete-level.error"));

            Popup::create(
                i18n::get_("shortcuts.uncomplete-level.title"),
                i18n::format("shortcuts.uncomplete-level.msg", level->m_levelName),
                i18n::get_("common.yes"), i18n::get_("common.no"),
                [level](bool yes) {
                    if (!yes) return;
                    auto gsm = utils::get<GameStatsManager>();
                    auto glm = utils::get<GameLevelManager>();
                    // xử lý xóa completion, stars, coins...
                    if (level->m_normalPercent >= 100 && gsm->hasCompletedLevel(level)) {
                        int levelid = level->m_levelID.value();
                        gsm->setStat("4", gsm->getStat("4") - 1);
                        auto levelKey = getLevelKey(level->m_levelID, level->m_levelType != GJLevelType::Main, level->m_dailyID > 0, level->m_gauntletLevel, level->m_dailyID > 200000);
                        gsm->m_completedLevels->removeObjectForKey(levelKey);
                        if (level->m_stars > 0) {
                            gsm->m_completedLevels->removeObjectForKey(fmt::format("{}", gsm->getStarLevelKey(level)));
                            gsm->m_completedLevels->removeObjectForKey(fmt::format("demon_{}", levelid));
                            if (level->isPlatformer()) gsm->setStat("28", gsm->getStat("28") - level->m_stars);
                            else gsm->setStat("6", gsm->getStat("6") - level->m_stars);
                            if (level->m_demon > 0) gsm->setStat("5", gsm->getStat("5") - 1);
                            for (auto i = 0; i < level->m_coins; i++) {
                                auto key = level->getCoinKey(i + 1);
                                if (gsm->hasUserCoin(key) && level->m_coinsVerified.value() > 0) {
                                    gsm->setStat("12", gsm->getStat("12") - 1);
                                }
                            }
                        }
                        for (auto i = 0; i < level->m_coins; i++) {
                            auto key = level->getCoinKey(i + 1);
                            if (level->m_coinsVerified.value() > 0 && gsm->hasUserCoin(key)) {
                                gsm->m_verifiedUserCoins->removeObjectForKey(key);
                            } else if (gsm->hasPendingUserCoin(key)) {
                                gsm->m_pendingUserCoins->removeObjectForKey(key);
                            }
                        }
                    }
                    level->m_practicePercent = 0;
                    level->m_normalPercent = 0;
                    level->m_newNormalPercent2 = 0;
                    level->m_orbCompletion = 0;
                    level->m_platformerSeed = 0;
                    level->m_bestPoints = 0;
                    level->m_bestTime = 0;
                    glm->saveLevel(level);
                }
            );
        }

        static void restartLevel() { if (auto* pl = utils::get<PlayLayer>()) pl->resetLevel(); }
        static void togglePracticeMode() { if (auto* pl = utils::get<PlayLayer>()) pl->togglePracticeMode(!pl->m_isPracticeMode); }
        static void placeCheckpoint() { if (auto* pl = utils::get<PlayLayer>()) if (pl->m_isPracticeMode) pl->markCheckpoint(); }
        static void removeCheckpoint() { if (auto* pl = utils::get<PlayLayer>()) if (pl->m_isPracticeMode) pl->removeCheckpoint(false); }

        #ifdef GEODE_IS_WINDOWS
        static void injectDll() {
            geode::utils::file::FilePickOptions::Filter filter;
            filter.description = "Dynamic Link Library (*.dll)";
            filter.files.insert("*.dll");
            geode::async::spawn(
                geode::utils::file::pick(geode::utils::file::PickMode::OpenFile, {geode::dirs::getGameDir(), {filter}}),
                [](geode::Result<std::optional<std::filesystem::path>> res) {
                    auto path = std::move(res).unwrapOrDefault().value_or("");
                    std::error_code ec;
                    if (path.empty() || !std::filesystem::exists(path, ec)) return;
                    HMODULE module = LoadLibraryW(path.native().c_str());
                    if (!module) return;
                    bool success = module > (HMODULE) HINSTANCE_ERROR;
                    if (success) {
                        using DllMain = BOOL(WINAPI*)(HINSTANCE, DWORD, LPVOID);
                        auto dllMain = reinterpret_cast<DllMain>(GetProcAddress(module, "DllMain"));
                        if (dllMain) dllMain(static_cast<HINSTANCE>(module), DLL_PROCESS_ATTACH, nullptr);
                    } else {
                        FreeLibrary(module);
                    }
                }
            );
        }
        #endif

        static void openSaveFolder() { geode::utils::file::openFolder(geode::Mod::get()->getSaveDir()); }
        static void resetBGVolume() {
            auto fmod = FMODAudioEngine::sharedEngine();
            fmod->setBackgroundMusicVolume(1.F);
            if (fmod->getBackgroundMusicVolume() <= 0.0) {
                if (auto GM = GameManager::sharedState()) GM->playMenuMusic();
            }
        }
        static void resetSFXVolume() { FMODAudioEngine::sharedEngine()->setEffectsVolume(1.F); }
        static void openDevtools() { ToggleDevToolsEvent().send(); }

        // recountSecretCoins, showLevelPassword, isJumpKey ... giữ nguyên như code gốc

        void init() override {
            // config mặc định cho keybinds
            config::setIfEmpty<keybinds::KeybindProps>("shortcut.p1jump", keybinds::Keys::None);
            config::setIfEmpty<keybinds::KeybindProps>("shortcut.p2jump", keybinds::Keys::None);

            auto tab = gui::MenuTab::find("tab.shortcuts");
            // thêm các button và keybinds như code gốc
            tab->addButton("shortcuts.options")->callback(openSettings)->handleKeybinds();
            tab->addButton("shortcuts.uncomplete-level")->callback(uncompleteLevel)->handleKeybinds();
            tab->addButton("shortcuts.restart-level")->callback(restartLevel)->handleKeybinds();
            tab->addButton("shortcuts.toggle-practice")->callback(togglePracticeMode)->handleKeybinds();
            tab->addButton("shortcuts.place-checkpoint")->callback(placeCheckpoint)->handleKeybinds();
            tab->addButton("shortcuts.remove-checkpoint")->callback(removeCheckpoint)->handleKeybinds();
            #ifdef GEODE_IS_WINDOWS
            tab->addButton("shortcuts.inject-dll")->callback(injectDll)->handleKeybinds();
            #endif
            tab->#include <modules/chatbot/ui/ChatboxPopup.hpp>
#include <modules/config/config.hpp>
#include <modules/gui/gui.hpp>
#include <modules/gui/components/button.hpp>
#include <modules/gui/components/keybind.hpp>
#include <modules/hack/hack.hpp>
#include <modules/i18n/translations.hpp>

struct ToggleDevToolsEvent : geode::Event<ToggleDevToolsEvent, bool()> {
    using Event::Event;
};

namespace superhack::Shortcuts {
    class $hack(Shortcuts) {
        using FileEvent = geode::Task<geode::Result<std::filesystem::path>>;

        static void openSettings() {
            if (auto* options = OptionsLayer::create()) {
                auto scene = utils::get<cocos2d::CCScene>();
                if (!scene) return;
                auto zOrder = scene->getHighestChildZ();
                scene->addChild(options, zOrder + 1);
                options->showLayer(false);
            }
        }

        static void openGraphicSettings() {
            #ifndef GEODE_IS_MACOS
            if (auto* options = VideoOptionsLayer::create()) {
                options->show();
            }
            #endif
        }

        static std::string getLevelKey(int levelID, bool isOnline, bool isDaily, bool isGauntlet, bool isEvent) {
            if (isOnline) return fmt::format("c_{}", levelID);
            if (isDaily) return fmt::format("d_{}", levelID);
            if (isGauntlet) return fmt::format("g_{}", levelID);
            if (isEvent) return fmt::format("e_{}", levelID);
            return fmt::format("n_{}", levelID);
        }

        static void uncompleteLevel() {
            auto scene = utils::get<cocos2d::CCScene>();
            if (!scene) return;
            GJGameLevel* level = nullptr;

            if (auto* pl = utils::get<PlayLayer>()) {
                level = pl->m_level;
            } else if (auto* lil = scene->getChildByType<LevelInfoLayer>(0)) {
                level = lil->m_level;
            }

            if (!level || level->m_levelType != GJLevelType::Saved)
                return Popup::create(i18n::get_("common.error"), i18n::get_("shortcuts.uncomplete-level.error"));

            Popup::create(
                i18n::get_("shortcuts.uncomplete-level.title"),
                i18n::format("shortcuts.uncomplete-level.msg", level->m_levelName),
                i18n::get_("common.yes"), i18n::get_("common.no"),
                [level](bool yes) {
                    if (!yes) return;
                    auto gsm = utils::get<GameStatsManager>();
                    auto glm = utils::get<GameLevelManager>();
                    // xử lý xóa completion, stars, coins...
                    if (level->m_normalPercent >= 100 && gsm->hasCompletedLevel(level)) {
                        int levelid = level->m_levelID.value();
                        gsm->setStat("4", gsm->getStat("4") - 1);
                        auto levelKey = getLevelKey(level->m_levelID, level->m_levelType != GJLevelType::Main, level->m_dailyID > 0, level->m_gauntletLevel, level->m_dailyID > 200000);
                        gsm->m_completedLevels->removeObjectForKey(levelKey);
                        if (level->m_stars > 0) {
                            gsm->m_completedLevels->removeObjectForKey(fmt::format("{}", gsm->getStarLevelKey(level)));
                            gsm->m_completedLevels->removeObjectForKey(fmt::format("demon_{}", levelid));
                            if (level->isPlatformer()) gsm->setStat("28", gsm->getStat("28") - level->m_stars);
                            else gsm->setStat("6", gsm->getStat("6") - level->m_stars);
                            if (level->m_demon > 0) gsm->setStat("5", gsm->getStat("5") - 1);
                            for (auto i = 0; i < level->m_coins; i++) {
                                auto key = level->getCoinKey(i + 1);
                                if (gsm->hasUserCoin(key) && level->m_coinsVerified.value() > 0) {
                                    gsm->setStat("12", gsm->getStat("12") - 1);
                                }
                            }
                        }
                        for (auto i = 0; i < level->m_coins; i++) {
                            auto key = level->getCoinKey(i + 1);
                            if (level->m_coinsVerified.value() > 0 && gsm->hasUserCoin(key)) {
                                gsm->m_verifiedUserCoins->removeObjectForKey(key);
                            } else if (gsm->hasPendingUserCoin(key)) {
                                gsm->m_pendingUserCoins->removeObjectForKey(key);
                            }
                        }
                    }
                    level->m_practicePercent = 0;
                    level->m_normalPercent = 0;
                    level->m_newNormalPercent2 = 0;
                    level->m_orbCompletion = 0;
                    level->m_platformerSeed = 0;
                    level->m_bestPoints = 0;
                    level->m_bestTime = 0;
                    glm->saveLevel(level);
                }
            );
        }

        static void restartLevel() { if (auto* pl = utils::get<PlayLayer>()) pl->resetLevel(); }
        static void togglePracticeMode() { if (auto* pl = utils::get<PlayLayer>()) pl->togglePracticeMode(!pl->m_isPracticeMode); }
        static void placeCheckpoint() { if (auto* pl = utils::get<PlayLayer>()) if (pl->m_isPracticeMode) pl->markCheckpoint(); }
        static void removeCheckpoint() { if (auto* pl = utils::get<PlayLayer>()) if (pl->m_isPracticeMode) pl->removeCheckpoint(false); }

        #ifdef GEODE_IS_WINDOWS
        static void injectDll() {
            geode::utils::file::FilePickOptions::Filter filter;
            filter.description = "Dynamic Link Library (*.dll)";
            filter.files.insert("*.dll");
            geode::async::spawn(
                geode::utils::file::pick(geode::utils::file::PickMode::OpenFile, {geode::dirs::getGameDir(), {filter}}),
                [](geode::Result<std::optional<std::filesystem::path>> res) {
                    auto path = std::move(res).unwrapOrDefault().value_or("");
                    std::error_code ec;
                    if (path.empty() || !std::filesystem::exists(path, ec)) return;
                    HMODULE module = LoadLibraryW(path.native().c_str());
                    if (!module) return;
                    bool success = module > (HMODULE) HINSTANCE_ERROR;
                    if (success) {
                        using DllMain = BOOL(WINAPI*)(HINSTANCE, DWORD, LPVOID);
                        auto dllMain = reinterpret_cast<DllMain>(GetProcAddress(module, "DllMain"));
                        if (dllMain) dllMain(static_cast<HINSTANCE>(module), DLL_PROCESS_ATTACH, nullptr);
                    } else {
                        FreeLibrary(module);
                    }
                }
            );
        }
        #endif

        static void openSaveFolder() { geode::utils::file::openFolder(geode::Mod::get()->getSaveDir()); }
        static void resetBGVolume() {
            auto fmod = FMODAudioEngine::sharedEngine();
            fmod->setBackgroundMusicVolume(1.F);
            if (fmod->getBackgroundMusicVolume() <= 0.0) {
                if (auto GM = GameManager::sharedState()) GM->playMenuMusic();
            }
        }
        static void resetSFXVolume() { FMODAudioEngine::sharedEngine()->setEffectsVolume(1.F); }
        static void openDevtools() { ToggleDevToolsEvent().send(); }

        // recountSecretCoins, showLevelPassword, isJumpKey ... giữ nguyên như code gốc

        void init() override {
            // config mặc định cho keybinds
            config::setIfEmpty<keybinds::KeybindProps>("shortcut.p1jump", keybinds::Keys::None);
            config::setIfEmpty<keybinds::KeybindProps>("shortcut.p2jump", keybinds::Keys::None);

            auto tab = gui::MenuTab::find("tab.shortcuts");
            // thêm các button và keybinds như code gốc
            tab->addButton("shortcuts.options")->callback(openSettings)->handleKeybinds();
            tab->addButton("shortcuts.uncomplete-level")->callback(uncompleteLevel)->handleKeybinds();
            tab->addButton("shortcuts.restart-level")->callback(restartLevel)->handleKeybinds();
            tab->addButton("shortcuts.toggle-practice")->callback(togglePracticeMode)->handleKeybinds();
            tab->addButton("shortcuts.place-checkpoint")->callback(placeCheckpoint)->handleKeybinds();
            tab->addButton("shortcuts.remove-checkpoint")->callback(removeCheckpoint)->handleKeybinds();
            #ifdef GEODE_IS_WINDOWS
            tab->addButton("shortcuts.inject-dll")->callback(injectDll)->handleKeybinds();
            #endif
            tab->addButton("shortcuts.open-save-folder")->callback(openSaveFolder)->handleKeybinds();
            tab->addButton("shortcuts.reset-bg-volume")->callback(resetBGVolume)->handleKeybinds();
            tab->addButton("shortcuts.reset-sfx-volume")->callback(resetSFXVolume)->    handleKeybinds();
            tab->addButton("shortcuts.open-devtools")->callback(openDevtools)->handleKey    binds();
             // ... thêm các button khác    }       }               