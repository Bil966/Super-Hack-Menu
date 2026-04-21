#include <modules/config/config.hpp>
#include <modules/gui/gui.hpp>
#include <modules/gui/components/button.hpp>
#include <modules/gui/components/combo.hpp>
#include <modules/gui/components/input-float.hpp>
#include <modules/gui/components/label-settings.hpp>
#include <modules/gui/components/toggle.hpp>
#include <modules/hack/hack.hpp>
#include <modules/i18n/translations.hpp>

#include <modules/labels/setting.hpp>
#include <modules/labels/variables.hpp>

#include <Geode/modify/PlayerObject.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/UILayer.hpp>
#include <modules/gui/cocos/cocos.hpp>

#include "Label.hpp"

namespace superhack::Labels {
    static std::vector<labels::LabelSettings> s_labels;

    // ... toàn bộ logic ClickStorage, LabelsPOHook, LabelsPLHook, LabelsUILHook giữ nguyên ...

    class $hack(Labels) {
        static void updateLabels(bool recreate = false) {
            auto* layer = utils::get<UILayer>();
            if (!layer) return;
            auto* labelsLayer = reinterpret_cast<LabelsUILHook*>(layer);
            labelsLayer->realignContainers(recreate);
        }

        static void refreshCocosUI() {
            if (auto cocos = gui::cocos::CocosRenderer::get()) {
                cocos->refreshPage();
            }
        }

        static std::vector<labels::LabelSettings> const DEFAULT_LABELS;

        void init() override {
            auto tab = gui::MenuTab::find("tab.labels");
            config::setIfEmpty("labels.visible", true);
            config::setIfEmpty("labels.cheat-indicator.endscreen", true);
            config::setIfEmpty("labels.cheat-indicator.scale", 0.5f);
            config::setIfEmpty("labels.cheat-indicator.opacity", 0.35f);

            s_labels = config::get<std::vector<labels::LabelSettings>>("labels", DEFAULT_LABELS);

            // ... thêm các toggle, button, combo như code gốc ...
            createLabelComponent();
        }

        void update() override {
            auto& manager = labels::VariableManager::get();
            manager.updateFPS();

            // cập nhật biến cho player 1 và player 2
            // ... giữ nguyên logic cleanup, setVariable ...
        }

        [[nodiscard]] char const* getId() const override { return "Labels"; }

        void createLabelComponent() {
            auto tab = gui::MenuTab::find("tab.labels");
            for (auto toggle : m_labelToggles) {
                tab->removeComponent(toggle);
            }
            m_labelToggles.clear();
            for (auto& setting : s_labels) {
                auto toggle = tab->addLabelSetting(&setting);
                // ... giữ nguyên logic deleteCallback, editCallback, exportCallback, moveCallback ...
                m_labelToggles.push_back(toggle);
            }
            refreshCocosUI();
        }

        std::vector<gui::LabelSettingsComponent*> m_labelToggles;
    };

    std::vector<labels::LabelSettings> const Labels::DEFAULT_LABELS = {
        {"FPS", "FPS: {round(fps)}", false},
        {"Testmode", "{isPracticeMode ? emojis.practice + 'Practice' : isTestMode ?? emojis.startPos + 'Testmode'}", false},
        {"Run From", "{!isPlatformer && (isPracticeMode || isTestMode) ?? 'From: ' + floor(runStart) + '%'}", false},
        {"Attempt", "Attempt {attempt}", false},
        {"Percentage", "{isPlatformer ? time : progress + '%'}", false},
        {"Level Time", "{time}", false},
        {"Best Run", "Best run: {runFrom}-{bestRun}%", false},
        {"Clock", "{clock}", false},
        {"CPS", "{cps}/{clicks}/{maxCps} CPS", false},
        {"Noclip Accuracy", "{ noclip ?? $'Accuracy: {noclipAccuracy}%'}", false},
        {"Noclip Deaths", "{ noclip ?? 'Deaths: ' + noclipDeaths}", false},
    };

    REGISTER_HACK(Labels)
}
