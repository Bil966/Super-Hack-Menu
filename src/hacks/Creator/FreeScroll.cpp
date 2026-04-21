#include <modules/config/config.hpp>
#include <modules/gui/gui.hpp>
#include <modules/gui/components/toggle.hpp>
#include <modules/hack/hack.hpp>

#include <Geode/modify/EditorUI.hpp>

namespace superhack::Creator {
    class $hack(FreeScroll) {
        void init() override {
            auto tab = gui::MenuTab::find("tab.creator");
            tab->addToggle("creator.freescroll")->handleKeybinds()->setDescription();
        }

        [[nodiscard]] const char* getId() const override { return "Free Scroll"; }
    };

    REGISTER_HACK(FreeScroll)

    class $modify(FreeScrollEUIHook, EditorUI) {
        ALL_DELEGATES_AND_SAFE_PRIO("creator.freescroll")

        void constrainGameLayerPosition(float width, float height) {
            // Hack: bỏ clamp, cho phép kéo tự do
            // Bình thường game sẽ ép objLayer->setPosition(...) vào trong giới hạn
            // Giờ ta override để không làm gì cả
        }
    };
}
