#include <modules/config/config.hpp>
#include <modules/gui/gui.hpp>
#include <modules/gui/components/toggle.hpp>
#include <modules/hack/hack.hpp>

#ifdef GEODE_IS_DESKTOP

#ifdef GEODE_IS_MACOS
#define CommentType CommentTypeDummy
#include <CoreGraphics/CoreGraphics.h>
#include <CoreServices/CoreServices.h>
#undef CommentType
#else
#include <Windows.h>
#endif

namespace superhack::Global {
    class $hack(LockCursor) {
        void init() override {
            auto tab = gui::MenuTab::find("tab.global");
            tab->addToggle("global.lockcursor")->setDescription()->handleKeybinds();
        }

        void update() override {
            auto* pl = utils::get<PlayLayer>();
            if (pl == nullptr) return;
            if (pl->m_hasCompletedLevel || pl->m_isPaused) return;
            if (!config::get<"global.lockcursor", bool>(false)) return;
            if (gui::Engine::get().isToggled()) return;

            GEODE_WINDOWS(
                HWND hwnd = WindowFromDC(wglGetCurrentDC());
                RECT winSize; GetWindowRect(hwnd, &winSize);
                auto width = winSize.right - winSize.left;
                auto height = winSize.bottom - winSize.top;
                auto centerX = width / 2 + winSize.left;
                auto centerY = height / 2 + winSize.top;
                SetCursorPos(centerX, centerY);
            )

            GEODE_MACOS(
                CGEventRef ourEvent = CGEventCreate(NULL);
                auto point = CGEventGetLocation(ourEvent);
                CFRelease(ourEvent);
                CGWarpMouseCursorPosition(point);
            )
        }

        [[nodiscard]] const char* getId() const override { return "Lock Cursor"; }
    };

    REGISTER_HACK(LockCursor)
}

#endif
