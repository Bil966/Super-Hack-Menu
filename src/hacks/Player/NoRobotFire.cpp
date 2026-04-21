#include <modules/config/config.hpp>
#include <modules/gui/gui.hpp>
#include <modules/gui/components/toggle.hpp>
#include <modules/hack/hack.hpp>

#include <Geode/modify/PlayerObject.hpp>

namespace superhack::Player {
    class $hack(NoRobotFire) {
        void init() override {
            auto tab = gui::MenuTab::find("tab.player");
            tab->addToggle("player.norobotfire")->setDescription()->handleKeybinds();
        }

        [[nodiscard]] const char* getId() const override { return "No Robot Fire"; }
    };

    REGISTER_HACK(NoRobotFire)

    class $modify(NoRobotFirePOHook, PlayerObject) {
        ADD_HOOKS_DELEGATE("player.norobotfire")

        void update(float dt) override {
            // Gọi update gốc
            PlayerObject::update(dt);

            // Tắt hiệu ứng lửa và particle burst của robot
            if (m_robotFire) m_robotFire->setVisible(false);
            if (m_robotBurstParticles) m_robotBurstParticles->setVisible(false);
        }
    };
}
