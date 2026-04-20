#include <modules.hpp>
#include <modules/bot/bot.hpp>
#include <modules/config/config.hpp>
#include <modules/gui/gui.hpp>
#include <modules/gui/popup.hpp>
#include <modules/gui/cocos/cocos.hpp>
#include <modules/gui/components/button.hpp>
#include <modules/gui/components/combo.hpp>
#include <modules/gui/components/filesystem-combo.hpp>
#include <modules/gui/components/radio.hpp>
#include <modules/gui/components/toggle.hpp>
#include <modules/hack/hack.hpp>
#include <modules/i18n/translations.hpp>

#include <Geode/modify/CheckpointObject.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/EditorUI.hpp>

using namespace geode::prelude;

namespace superhack::Bot {
    static bot::Bot s_bot;
    static bool s_dontPlaceAuto = false;
    bot::Bot& getBot() { return s_bot; }

    // Replay management
    void newReplay();
    void saveReplay();
    void confirmLoad(std::filesystem::path const& replayPath);
    void loadReplay();
    void deleteReplay();
    void openReplaysFolder();

    class $hack(Bot) {
        static void savePreBotSettings();
        static void restorePreBotSettings();
        static void applySettings();

        void init() override {
            const auto updateBotState = [](int state) {
                if (s_bot.getState() == bot::State::DISABLED) {
                    savePreBotSettings();
                }
                s_bot.setState(static_cast<bot::State>(state));
                if (state == 0) {
                    restorePreBotSettings();
                    return;
                }
                Bot::applySettings();
            };

#ifdef GEODE_IS_WINDOWS
            config::set("bot.practice-fix-mode", 0);
#else
            config::set("bot.practice-fix-mode", 0);
#endif
            config::set("bot.state", 0);
            restorePreBotSettings();
            updateBotState(0);

            auto tab = gui::MenuTab::find("tab.bot");

            tab->addRadioButton("bot.disabled", "bot.state", 0)->callback(updateBotState)->handleKeybinds();
            tab->addRadioButton("bot.record", "bot.state", 1)->callback(updateBotState)->handleKeybinds();
            tab->addRadioButton("bot.playback", "bot.state", 2)->callback(updateBotState)->handleKeybinds();

            tab->addFilesystemCombo("bot.replays", "bot.selectedreplay", Mod::get()->getSaveDir() / "replays");
#ifdef GEODE_IS_WINDOWS
            tab->addCombo("bot.practice-fix-mode", {"Checkpoint", "Memory"}, 0)->setDescription();
#endif

            tab->addToggle("bot.ignore-inputs")->handleKeybinds()->setDescription();

            tab->addButton("common.new")->handleKeybinds()->callback(newReplay);
            tab->addButton("common.save")->handleKeybinds()->callback(saveReplay);
            tab->addButton("common.load")->handleKeybinds()->callback(loadReplay);
            tab->addButton("common.delete")->handleKeybinds()->callback(deleteReplay);
            tab->addButton("bot.open-replays-folder")->handleKeybinds()->callback(openReplaysFolder);
        }

        [[nodiscard]] bool isCheating() const override {
            auto state = config::get<"bot.state", int>(0);
            return state == 2 && s_bot.getInputCount() != 0;
        }

        [[nodiscard]] const char* getId() const override { return "Bot"; }
    };

    REGISTER_HACK(Bot)

    // Hooks
    class $modify(BotPLHook, PlayLayer) {
        bool init(GJGameLevel* gj, bool p1, bool p2) {
            bool result = PlayLayer::init(gj, p1, p2);
            s_bot.setLevelInfo(gdr::Level(gj->m_levelName, gj->m_levelID.value()));
            s_bot.setPlatformer(gj->isPlatformer());
            return result;
        }

        void resetLevel() {
            PlayLayer::resetLevel();
            Bot::applySettings();
            if (m_checkpointArray->count() == 0) {
                if(s_bot.getState() != bot::State::PLAYBACK)
                    s_bot.clearInputs();
                s_bot.restart();
            }
        }

        CheckpointObject* markCheckpoint() {
            if (s_bot.getState() == bot::State::RECORD && (m_player1->m_isDead || m_player2->m_isDead))
                return nullptr;
            return PlayLayer::markCheckpoint();
        }

        void loadFromCheckpoint(CheckpointObject* checkpoint) {
            PlayLayer* playLayer = utils::get<PlayLayer>();
            if (s_bot.getState() != bot::State::RECORD || !playLayer)
                return PlayLayer::loadFromCheckpoint(checkpoint);
            s_bot.removeInputsAfter(checkpoint->m_gameState.m_currentProgress / 2);
            PlayLayer::loadFromCheckpoint(checkpoint);
        }
    };

    class $modify(BotPlayerHook, PlayerObject) {
        struct Fields { bool m_triedPlacingCheckpoint = false; };

        static void onModify(auto& self) {
            int value = config::get("bot.state", 0);
            geode::Hook* hookPtr = nullptr;
            auto it = self.m_hooks.find("PlayerObject::tryPlaceCheckpoint");
            if (it != self.m_hooks.end()) {
                it->second->setAutoEnable(value == (int)bot::State::RECORD);
                it->second->setPriority(SAFE_HOOK_PRIORITY);
                hookPtr = it->second.get();
            }
            config::addDelegate("bot.state", [hookPtr] {
                int value = config::get("bot.state", 0);
                (void) hookPtr->toggle(value == (int)bot::State::RECORD);
            });
        }

        void tryPlaceCheckpoint() {
            if(s_dontPlaceAuto) {
                m_fields->m_triedPlacingCheckpoint = true;
                return;
            }
            PlayerObject::tryPlaceCheckpoint();
        }
    };

    class $modify(BotBGLHook, GJBaseGameLayer) {
        static void onModify(auto& self) {
            SAFE_HOOKS(GJBaseGameLayer, "processQueuedButtons");
            int value = config::get("bot.state", 0);
            geode::Hook* hookPtr = nullptr;
            auto it = self.m_hooks.find("GJBaseGameLayer::update");
            if (it != self.m_hooks.end()) {
                it->second->setAutoEnable(value == (int)bot::State::RECORD);
                it->second->setPriority(SAFE_HOOK_PRIORITY);
                hookPtr = it->second.get();
            }
            config::addDelegate("bot.state", [hookPtr] {
                int value = config::get("bot.state", 0);
                (void) hookPtr->toggle(value == (int)bot::State::RECORD);
            });
        }

        void update(float dt) {
            auto player1Fields = reinterpret_cast<BotPlayerHook*>(m_player1)->m_fields.self();
            auto player2Fields = reinterpret_cast<BotPlayerHook*>(m_player2)->m_fields.self();

            player1Fields->m_triedPlacingCheckpoint = false;
            player2Fields->m_triedPlacingCheckpoint = false;
            s_dontPlaceAuto = true;
            GJBaseGameLayer::update(dt);
            s_dontPlaceAuto = false;

            if(player1Fields->m_triedPlacingCheckpoint) {
                m_player1->m_shouldTryPlacingCheckpoint = true;
                m_player1->tryPlaceCheckpoint();
            }
            if(player2Fields->m_triedPlacingCheckpoint) {
                m_player2->m_shouldTryPlacingCheckpoint = true;
                m_player2->tryPlaceCheckpoint();
            }
        }

        void simulateClick(PlayerButton button, bool down, bool player2);
        void processBot();
        void processQueuedButtons(float dt, bool clearInputQueue);
        void handleButton(bool down, int button, bool player1);
    };

    class $modify(BotEUIHook, EditorUI) {
        void onPlaytest(CCObject* sender) {
            if (auto* editorLayer = utils::get<LevelEditorLayer>()) {
                if (editorLayer->m_playbackMode == PlaybackMode::Not) {
                    s_bot.restart();
                    editorLayer->m_gameState.m_currentProgress = 0;
                }
            }
            EditorUI::onPlaytest(sender);
        }
    };
}
