#include <modules/api/mods.hpp>
#include <modules/config/config.hpp>
#include <modules/gui/color.hpp>
#include <modules/gui/gui.hpp>
#include <modules/gui/cocos/nodes/CCMenuItemExt.hpp>
#include <modules/gui/components/toggle.hpp>
#include <modules/hack/hack.hpp>

#include <Geode/binding/GameStatsManager.hpp>

#include <Geode/modify/EditLevelLayer.hpp>
#include <Geode/modify/EndLevelLayer.hpp>
#include <Geode/modify/LevelInfoLayer.hpp>
#include <Geode/modify/LevelPage.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/RetryLevelLayer.hpp>

#include <modules.hpp>
#include <ranges>

namespace superhack::Global {
    enum class SafeModeState {
        Normal,
        Cheating,
        Tripped
    };

    std::map<std::string_view, bool> s_attemptCheats;
    std::map<std::string_view, bool> const& getAttemptCheats() { return s_attemptCheats; }

    bool s_trippedLastAttempt = false;

    class $hack(AutoSafeMode) {
        static bool hasCheats();
        static bool shouldEnable();
        static void updateCheatStates();
        static std::string constructMessage();
        static void showPopup(std::string const& message);

        void init() override;
        void update() override;
        [[nodiscard]] const char* getId() const override { return "Auto Safe Mode"; }
    };

    class $hack(SafeMode) {
        void init() override;
        [[nodiscard]] const char* getId() const override { return "Safe Mode"; }
    };

    REGISTER_HACK(AutoSafeMode)
    REGISTER_HACK(SafeMode)

    class $modify(SafeModePLHook, PlayLayer) {
        struct Fields {
            std::uint32_t totalJumps;
            std::uint32_t totalAttempts;
        };

        ENABLE_SAFE_HOOKS_ALL()

        bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects);
        void destroyPlayer(PlayerObject* player, GameObject* object) override;
        void resetLevel() override;
        void levelComplete();
    };

    class $modify(SafeModePOHook, PlayerObject) {
        ENABLE_SAFE_HOOKS_ALL()
        void incrementJumps();
    };

    #define NormalColor gui::Colors::GREEN
    #define CheatingColor gui::Colors::RED
    #define TrippedColor gui::Color { 0.72f, 0.37f, 0.f }

    static CCMenuItemSpriteExtra* createCI();

    class $modify(SafeModeRLLHook, RetryLevelLayer) {
        void customSetup() override;
    };

    class $modify(SafeModeELLHook, EndLevelLayer) {
        ENABLE_FIRST_HOOKS_ALL()
        void customSetup() override;
    };

    static bool manualSafeModePopup();
    static bool showCheatWarn();

    #define DEFINE_WARN_HOOK(name, cls)                \
    class $modify(name, cls) {                         \
        ENABLE_FIRST_HOOKS_ALL()                       \
        struct Fields { bool m_shownCheatWarn = false; }; \
        void onPlay(CCObject* sender) {                \
            if (!m_fields->m_shownCheatWarn) {         \
                m_fields->m_shownCheatWarn = true;     \
                if (showCheatWarn()) return;           \
            }                                          \
            cls::onPlay(sender);                       \
        }                                              \
    }

    DEFINE_WARN_HOOK(CheatsEnabledWarnLPHook, LevelPage);
    DEFINE_WARN_HOOK(CheatsEnabledWarnELLHook, EditLevelLayer);
    DEFINE_WARN_HOOK(CheatsEnabledWarnLILHook, LevelInfoLayer);
}
