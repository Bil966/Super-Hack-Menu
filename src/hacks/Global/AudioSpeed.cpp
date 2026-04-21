#include <modules/config/config.hpp>
#include <modules/gui/gui.hpp>
#include <modules/gui/components/float-toggle.hpp>
#include <modules/gui/components/toggle.hpp>
#include <modules/hack/hack.hpp>

#include <Geode/modify/FMODAudioEngine.hpp>

namespace superhack::Global {
    class $hack(AudioSpeed) {
        void init() override {
            auto tab = gui::MenuTab::find("tab.global");

            config::setIfEmpty("global.audiospeed.toggle", false);
            config::setIfEmpty("global.audiospeed", 1.f);

            tab->addFloatToggle("global.audiospeed", 0.0001f, 1000.f, "%.4f")->handleKeybinds();
            tab->addToggle("global.audiospeed.sync")->handleKeybinds()->setDescription();
        }

        [[nodiscard]] const char* getId() const override { return "Audio Speed"; }
        [[nodiscard]] int32_t getPriority() const override { return -8; }

        bool m_lastSpeedhackState = false;
    };

    REGISTER_HACK(AudioSpeed)

    class $modify(AudioSpeedFMODHook, FMODAudioEngine) {
        static bool shouldUpdate() {
            static bool lastSpeedhack = false;

            auto speedhack = config::get<"global.speedhack.toggle", bool>();
            auto audioSpeed = config::get<"global.audiospeed.toggle", bool>();
            auto syncWithSpeedhack = config::get<"global.audiospeed.sync", bool>();

            bool toggleSpeedhack = lastSpeedhack != speedhack;
            lastSpeedhack = speedhack;

            return (speedhack || toggleSpeedhack) && syncWithSpeedhack || audioSpeed;
        }

        static float getSpeed() {
            if (config::get<"global.audiospeed.toggle", bool>()) {
                return config::get<"global.audiospeed", double>(1.f);
            }

            if (config::get<"global.audiospeed.sync", bool>()) {
                auto speedhack = config::get<"global.speedhack", double>(1.f);
                auto speedhackToggle = config::get<"global.speedhack.toggle", bool>();
                return speedhackToggle ? speedhack : 1.f;
            }

            return 1.f;
        }

        void update(float dt) override {
            FMODAudioEngine::update(dt);
            if (!shouldUpdate()) return;

            FMOD::ChannelGroup* masterGroup;
            if (m_system->getMasterChannelGroup(&masterGroup) != FMOD_OK) return;
            masterGroup->setPitch(getSpeed());
        }
    };
}
