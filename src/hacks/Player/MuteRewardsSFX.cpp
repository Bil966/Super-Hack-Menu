#include <modules/config/config.hpp>
#include <modules/gui/gui.hpp>
#include <modules/gui/components/toggle.hpp>
#include <modules/hack/hack.hpp>

#include <Geode/modify/FMODAudioEngine.hpp>

namespace superhack::Player {
    class $hack(MuteRewardsSFX) {
        void init() override {
            auto tab = gui::MenuTab::find("tab.player");
            tab->addToggle("player.muterewardssfx")->setDescription()->handleKeybinds();
        }

        [[nodiscard]] const char* getId() const override { return "Mute Rewards SFX on Death"; }
    };

    REGISTER_HACK(MuteRewardsSFX)

    constexpr std::array<std::string_view, 4> badSFX = {
        "achievement_01.ogg", "magicExplosion.ogg", "gold02.ogg", "secretKey.ogg"
    };

    class $modify(MuteRewardsSFXFMODAEHook, FMODAudioEngine) {
        static void onModify(auto& self) {
            FIRST_PRIORITY("FMODAudioEngine::playEffect");
            HOOKS_TOGGLE_ALL("player.muterewardssfx");
        }

        int playEffect(gd::string path, float speed, float p2, float volume) {
            auto* pl = utils::get<PlayLayer>();

            // Nếu không ở trong PlayLayer hoặc player chưa chết hoặc đang pause → cho phép play
            if (!pl || !pl->m_player1 || !pl->m_player1->m_isDead || pl->m_isPaused)
                return FMODAudioEngine::playEffect(path, speed, p2, volume);

            // Nếu SFX không nằm trong danh sách badSFX → cho phép play
            std::string sfxPath(path);
            if (std::find(badSFX.begin(), badSFX.end(), sfxPath) == badSFX.end())
                return FMODAudioEngine::playEffect(path, speed, p2, volume);

            // Nếu player đã chết và SFX nằm trong badSFX → chặn
            return 0;
        }
    };
}
