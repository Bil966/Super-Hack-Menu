#include <modules/config/config.hpp>
#include <modules/gui/gui.hpp>
#include <modules/gui/components/toggle.hpp>
#include <modules/hack/hack.hpp>

#include <Geode/modify/PlayerObject.hpp>

namespace superhack::Player {
    class $hack(NoRespawnFlash) {
        void init() override {
            auto tab = gui::MenuTab::find("tab.player");
            tab->addToggle("player.norespawnflash")->setDescription()->handleKeybinds();
        }

        [[nodiscard]] const char* getId() const override { return "No Respawn Flash"; }
    };

    REGISTER_HACK(NoRespawnFlash)

    class $modify(NoRespawnFlashPOHook, PlayerObject) {
        ALL_DELEGATES_AND_SAFE_PRIO("player.norespawnflash")

        // Vô hiệu hóa hiệu ứng flash khi respawn
        void playSpawnEffect() {}
    };
}
