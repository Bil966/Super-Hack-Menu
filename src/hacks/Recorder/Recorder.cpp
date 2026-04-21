#include <modules/config/config.hpp>
#include <modules/gui/gui.hpp>
#include <modules/gui/components/button.hpp>
#include <modules/gui/components/combo.hpp>
#include <modules/hack/hack.hpp>
#include <modules/recorder/recorder.hpp>

#include <Geode/modify/CCScheduler.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/ShaderLayer.hpp>
#include <modules/gui/cocos/components/ToggleComponent.hpp>

#include <modules/i18n/translations.hpp>
#include <modules/recorder/DSPRecorder.hpp>

#ifdef GEODE_IS_ANDROID
#define STR(x) std::string(x)
#else
#define STR(x) x
#endif

namespace superhack::Recorder {
    static recorder::Recorder s_recorder;

    bool levelDone = false;
    bool popupShown = false;
    bool capturing = false;

    float totalTime = 0.f;
    float afterEndTimer = 0.f;
    float extraTime = 0.f;
    float lastFrameTime = 0.f;

    cocos2d::CCSize oldDesignResolution;
    cocos2d::CCSize newDesignResolution;
    cocos2d::CCSize originalScreenScale;
    cocos2d::CCSize newScreenScale;

    void callback(std::string const& error) {
        geode::queueInMainThread([error] {
            Popup::create(i18n::get_("common.error"), error);
        });
    }

    void endPopup() {
        Popup::create(
            i18n::get_("common.info"),
            i18n::format("recorder.finished", s_recorder.getRecordingDuration()),
            i18n::get_("common.ok"),
            i18n::get_("recorder.open-folder"),
            [](bool result) {
                if (result) return;
                geode::utils::file::openFolder(geode::Mod::get()->getSaveDir() / "renders");
            }
        );
    }

    void applyWinSize() {
        if (newDesignResolution.width != 0 && newDesignResolution.height != 0) {
            auto view = utils::get<cocos2d::CCEGLView>();
            utils::get<cocos2d::CCDirector>()->m_obWinSizeInPoints = newDesignResolution;
            view->setDesignResolutionSize(newDesignResolution.width, newDesignResolution.height, ResolutionPolicy::kResolutionExactFit);
            view->m_fScaleX = newScreenScale.width;
            view->m_fScaleY = newScreenScale.height;
        }
    }

    void restoreWinSize() {
        if (oldDesignResolution.width != 0 && oldDesignResolution.height != 0) {
            auto view = utils::get<cocos2d::CCEGLView>();
            utils::get<cocos2d::CCDirector>()->m_obWinSizeInPoints = oldDesignResolution;
            view->setDesignResolutionSize(oldDesignResolution.width, oldDesignResolution.height, ResolutionPolicy::kResolutionExactFit);
            view->m_fScaleX = originalScreenScale.width;
            view->m_fScaleY = originalScreenScale.height;
        }
    }

    void fixUIObjects() {
        auto pl = utils::get<PlayLayer>();
        for (auto obj : geode::cocos::CCArrayExt<GameObject*>(pl->m_objects)) {
            auto it = pl->m_uiObjectPositions.find(obj->m_uniqueID);
            if (it == pl->m_uiObjectPositions.end()) continue;
            obj->setStartPos(it->second);
        }
        pl->positionUIObjects();
    }

    inline void trimString(std::string& str) {
        str.erase(std::ranges::unique(str, [](char a, char b) {
            return std::isspace(a) && std::isspace(b);
        }).begin(), str.end());
    }

    void start() {
        if (!utils::get<PlayLayer>()) return;
        levelDone = false;
        popupShown = false;
        totalTime = 0.f;
        extraTime = 0.f;
        lastFrameTime = 0.f;
        afterEndTimer = 0.f;

        GJGameLevel* lvl = utils::get<PlayLayer>()->m_level;
        std::string trimmedLevelName = lvl->m_levelName;
        std::erase(trimmedLevelName, '/');
        std::erase(trimmedLevelName, '\\');
        trimString(trimmedLevelName);

        std::filesystem::path renderDirectory = geode::Mod::get()->getSaveDir() / "renders" / STR(trimmedLevelName);
        std::error_code ec;
        if (!std::filesystem::exists(renderDirectory, ec)) {
            std::filesystem::create_directories(renderDirectory, ec);
            if (ec) return Popup::create(i18n::get_("common.error"), ec.message());
        }

        s_recorder.m_renderSettings.m_bitrate = static_cast<int>(config::get<double>("recorder.bitrate", 30.f)) * 1000000;
        s_recorder.m_renderSettings.m_fps = static_cast<int>(config::get<double>("recorder.fps", 60.f));
        s_recorder.m_renderSettings.m_width = config::get<int>("recorder.resolution.x", 1920);
        s_recorder.m_renderSettings.m_height = config::get<int>("recorder.resolution.y", 1080);
        s_recorder.m_renderSettings.m_codec = config::get<std::string>("recorder.codecString", "libx264");
        s_recorder.m_renderSettings.m_outputFile = renderDirectory / (fmt::format("{} - {}.mp4", trimmedLevelName, lvl->m_levelID.value()));
        s_recorder.m_renderSettings.m_hardwareAccelerationType = static_cast<ffmpeg::HardwareAccelerationType>(config::get<int>("recorder.hwType", 0));
        s_recorder.m_renderSettings.m_colorspaceFilters = config::get<std::string>("recorder.colorspace", "");
        s_recorder.m_renderSettings.m_doVerticalFlip = false;

        auto view = utils::get<cocos2d::CCEGLView>();
        oldDesignResolution = view->getDesignResolutionSize();
        float aspectRatio = static_cast<float>(s_recorder.m_renderSettings.m_width) / static_cast<float>(s_recorder.m_renderSettings.m_height);
        newDesignResolution = cocos2d::CCSize(roundf(320.f * aspectRatio), 320.f);

        originalScreenScale = cocos2d::CCSize(view->m_fScaleX, view->m_fScaleY);
        auto retinaRatio = geode::utils::getDisplayFactor();
        newScreenScale = cocos2d::CCSize(
            static_cast<float>(s_recorder.m_renderSettings.m_width) / newDesignResolution.width / retinaRatio,
            static_cast<float>(s_recorder.m_renderSettings.m_height) / newDesignResolution.height / retinaRatio
        );

        if (oldDesignResolution != newDesignResolution) {
            applyWinSize();
            fixUIObjects();
        }

        s_recorder.start();
    }

    void stop() { s_recorder.stop(); }

    class $hack(InternalRecorder) {
        void init() override {}
        void lateInit() override {
            auto ffmpeg = geode::Loader::get()->getLoadedMod("eclipse.ffmpeg-api");
            if (!ffmpeg) return;
            if (ffmpeg->getVersion() < geode::VersionInfo(1, 2, 0)) return;

            s_recorder.setCallback(callback);
            auto tab = gui::MenuTab::find("tab.recorder");
            tab->addButton("recorder.start")->callback(start);
            tab->addButton("recorder.stop")->callback([] { if (s_recorder.isRecording()) stop(); });
            // thêm các input config như code gốc...
        }
        [[nodiscard]] const char* getId() const override { return "Internal Recorder"; }
        std::vector<std::string> m_codecs;
    };

    REGISTER_HACK(InternalRecorder)

    class $modify(InternalRecorderSLHook, ShaderLayer) {
        void visit() {
            if (s_recorder.isRecording()) {
                setScaleY(-1);
                ShaderLayer::visit();
                return setScaleY(1);
            }
            ShaderLayer::visit();
        }
    };

    class $modify(InternalRecorderSchedulerHook, cocos2d::CCScheduler) {
        ENABLE_SAFE_HOOKS_ALL()
        void update(float dt) {
            if (s_recorder.isRecording()) {
                float framerate = config::get<"recorder.fps", double>(60.f);
                if (framerate < 1) framerate = 1;
                double tps = utils::getTPS();
                dt = 1.f / framerate;
                dt *= framerate / tps;
                applyWinSize();
                CCScheduler::update(dt);
                restoreWinSize();
                return;