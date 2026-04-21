#include <modules/config/config.hpp>
#include <modules/gui/gui.hpp>
#include <modules/gui/components/toggle.hpp>
#include <modules/hack/hack.hpp>

#include <Geode/modify/EffectGameObject.hpp>
#include <Geode/modify/PlayerObject.hpp>

namespace superhack::Player {
    class $hack(MuteLevelAudioOnDeath) {
        void init() override {
            auto tab = gui::MenuTab::find("tab.player");
            tab->addToggle("player.mutelevelaudioondeath")->setDescription()->handleKeybinds();
        }

        [[nodiscard]] const char* getId() const override { return "Mute Level Audio On Death"; }
    };

    REGISTER_HACK(MuteLevelAudioOnDeath)

    class $modify(MuteLevelAudioOnDeathPOHook, PlayerObject) {
        ADD_HOOKS_DELEGATE("player.mutelevelaudioondeath")

        void playerDestroyed(bool p0) {
            PlayLayer* pl = utils::get<PlayLayer>();
            if (!pl) return PlayerObject::playerDestroyed(p0);

            // chỉ xử lý cho player1 hoặc player2
            if (this != pl->m_player1 && this != pl->m_player2)
                return PlayerObject::playerDestroyed(p0);

            // nếu đang ở practice mode nhưng không sync nhạc thì bỏ qua
            if (pl->m_isPracticeMode && !pl->m_practiceMusicSync)
                return PlayerObject::playerDestroyed(p0);

            const auto fmod = utils::get<FMODAudioEngine>();
            if (!fmod) return PlayerObject::playerDestroyed(p0);

            // phân biệt platformer và classic
            if (pl->m_isPlatformer) fmod->pauseAllMusic(true);
            else fmod->stopAllMusic(true);

            // tránh dừng SFX hai lần trong chế độ 2P
            if (this == pl->m_player2 && pl->m_level->m_twoPlayerMode)
                return PlayerObject::playerDestroyed(p0);

            // dừng toàn bộ hiệu ứng để giữ lại death SFX
            fmod->stopAllEffects();

            PlayerObject::playerDestroyed(p0);
        }
    };

    class $modify(MuteLevelAudioOnDeathEGOHook, EffectGameObject) {
        ADD_HOOKS_DELEGATE("player.mutelevelaudioondeath")

        void triggerObject(GJBaseGameLayer* p0, int p1, gd::vector<int> const* p2) {
            PlayLayer* pl = utils::get<PlayLayer>();
            if (!pl) return EffectGameObject::triggerObject(p0, p1, p2);

            PlayerObject* player = pl->m_player1;
            if (!player || !player->m_isDead) 
                return EffectGameObject::triggerObject(p0, p1, p2);

            int id = this->m_objectID;
            // chặn trigger spawn nhạc/sfx khi player đã chết
            if (player->m_isDead && (id == 3602 || id == 1934)) {
                return; // không gọi triggerObject nữa
            }

            return EffectGameObject::triggerObject(p0, p1, p2);
        }
    };
}
