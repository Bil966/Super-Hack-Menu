#include <Geode/platform/cplatform.h>
#ifdef GEODE_IS_DESKTOP
#include <modules/config/config.hpp>
#include <modules/gui/gui.hpp>
#include <modules/gui/components/combo.hpp>
#include <modules/gui/components/input-float.hpp>
#include <modules/gui/components/input-text.hpp>
#include <modules/gui/components/toggle.hpp>
#include <modules/hack/hack.hpp>
#include <modules/i18n/translations.hpp>
#include <modules/labels/variables.hpp>

#include <discord-rpc.hpp>
#include <rift.hpp>

#include <Geode/modify/GJBaseGameLayer.hpp>

namespace superhack::Global {
    static std::chrono::time_point<std::chrono::steady_clock> s_lastDiscordUpdate;
    static geode::utils::StringMap<std::unique_ptr<rift::Script>> s_discordScripts;
    static time_t s_startTimestamp, s_levelTimestamp;

    class $hack(DiscordRPC) {
        constexpr static auto DEFAULT_CLIENT_ID = "1212016614325624852";

        enum class GameState { Menu, Editor, Level, Platformer };

        static GameState getGameState() {
            if (auto* pl = utils::get<PlayLayer>())
                return pl->m_isPlatformer ? GameState::Platformer : GameState::Level;
            if (auto* ed = utils::get<LevelEditorLayer>())
                return GameState::Editor;
            return GameState::Menu;
        }

        static void refreshPresence() {
            auto gameState = getGameState();
            auto getScript = [gameState](const std::string& key, bool addPrefix = true) -> rift::Script* {
                static auto nullScript = rift::compile("").unwrap();
                std::string_view keyStr;
                if (addPrefix) {
                    switch (gameState) {
                        case GameState::Menu: keyStr = "menu."; break;
                        case GameState::Editor: keyStr = "editor."; break;
                        case GameState::Level: keyStr = "level."; break;
                        case GameState::Platformer: keyStr = "plat."; break;
                    }
                }
                auto it = s_discordScripts.find(fmt::format("{}{}", keyStr, key));
                return it != s_discordScripts.end() ? it->second.get() : nullScript.get();
            };

            auto& varManager = labels::VariableManager::get();
            varManager.refetch();
            const auto& vars = varManager.getVariables();
            auto& presence = discord::RPCManager::get().getPresence();
            presence
                .setState(getScript("state")->run(vars).unwrapOrDefault())
                .setDetails(getScript("details")->run(vars).unwrapOrDefault())
                .setLargeImageKey(getScript("largeimage")->run(vars).unwrapOrDefault())
                .setLargeImageText(getScript("largeimage.text")->run(vars).unwrapOrDefault())
                .setSmallImageKey(getScript("smallimage")->run(vars).unwrapOrDefault())
                .setSmallImageText(getScript("smallimage.text")->run(vars).unwrapOrDefault())
                .setButton1(getScript("button1.text", false)->run(vars).unwrapOrDefault(),
                            getScript("button1.url", false)->run(vars).unwrapOrDefault(),
                            config::get<bool>("global.discordrpc.button1.enabled", false))
                .setButton2(getScript("button2.text", false)->run(vars).unwrapOrDefault(),
                            getScript("button2.url", false)->run(vars).unwrapOrDefault(),
                            config::get<bool>("global.discordrpc.button2.enabled", false));

            switch (config::get<int>("global.discordrpc.timemode", 1)) {
                default: break;
                case 1: presence.setStartTimestamp(s_startTimestamp); break;
                case 2: if (gameState != GameState::Menu) presence.setStartTimestamp(s_levelTimestamp); break;
                case 3: presence.setStartTimestamp(gameState == GameState::Menu ? s_startTimestamp : s_levelTimestamp); break;
            }

            presence.refresh();
        }

        static void toggleRPC(bool enabled) {
            if (enabled) refreshPresence();
            else discord::RPCManager::get().clearPresence();
        }

        static void initializeDiscord() {
            static bool initialized = false;
            if (initialized) {
                discord::RPCManager::get().shutdown();
                initialized = false;
            }
            geode::log::info("Initializing Discord RPC");
            discord::RPCManager::get()
                .setClientID(config::get<std::string>("global.discordrpc.clientid", DEFAULT_CLIENT_ID))
                .onReady([](auto& user) { geode::log::info("Discord RPC ready: {}", user.username); })
                .onErrored([](auto code, auto message) { geode::log::error("Discord RPC error: {} ({})", message, code); })
                .initialize();
            initialized = true;
        }

        static void compileScript(const std::string& name) {
            auto res = rift::compile(config::get<std::string>("global.discordrpc." + name, ""));
            if (res.isOk()) s_discordScripts[name] = std::move(res.unwrap());
        }

        static void recompileScripts() {
            s_discordScripts.clear();
            compileScript("menu.state"); compileScript("menu.details"); compileScript("menu.largeimage");
            compileScript("menu.largeimage.text"); compileScript("menu.smallimage"); compileScript("menu.smallimage.text");
            compileScript("editor.state"); compileScript("editor.details"); compileScript("editor.largeimage");
            compileScript("editor.largeimage.text"); compileScript("editor.smallimage"); compileScript("editor.smallimage.text");
            compileScript("level.state"); compileScript("level.details"); compileScript("level.largeimage");
            compileScript("level.largeimage.text"); compileScript("level.smallimage"); compileScript("level.smallimage.text");
            compileScript("plat.state"); compileScript("plat.details"); compileScript("plat.largeimage");
            compileScript("plat.largeimage.text"); compileScript("plat.smallimage"); compileScript("plat.smallimage.text");
            compileScript("button1.text"); compileScript("button2.text"); compileScript("button1.url"); compileScript("button2.url");
        }

        void init() override {
            auto tab = gui::MenuTab::find("tab.global");
            config::setIfEmpty<std::string>("global.discordrpc.clientid", DEFAULT_CLIENT_ID);
            config::setIfEmpty("global.discordrpc.interval", 250.0f);
            config::setIfEmpty("global.discordrpc.timemode", 1);
            s_startTimestamp = std::time(nullptr);
            s_levelTimestamp = std::time(nullptr);

            // default scripts (menu, editor, level, platformer, buttons) giữ nguyên như bản gốc...

            initializeDiscord();
            recompileScripts();

            tab->addToggle("global.discordrpc")
               ->handleKeybinds()
               ->callback([this](bool enabled) { toggleRPC(enabled); })
               ->setDescription()
               ->addOptions([this](auto opt) {
                   opt->addInputText("global.discordrpc.client-id", "global.discordrpc.clientid");
                   opt->addInputFloat("global.discordrpc.update-rate", "global.discordrpc.interval", 100, FLT_MAX, "%.0f")->setDescription();
                   opt->addCombo("global.discordrpc.time-mode", "global.discordrpc.timemode",
                       { i18n::get_("global.discordrpc.time-mode.0"), i18n::get_("global.discordrpc.time-mode.1"),
                         i18n::get_("global.discordrpc.time-mode.2"), i18n::get_("global.discordrpc.time-mode.3") },
                       config::get<int>("global.discordrpc.timemode", 1))->setDescription();
                   // thêm các input text cho script như bản gốc...
               });
        }

        void update() override {
            if (!config::get<"global.discordrpc", bool>(false)) return;
            auto interval = config::get<"global.discordrpc.interval", double>(200.0f);
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - s_lastDiscordUpdate).count();
            if (static_cast<float>(elapsed) >= interval) {
                refreshPresence();
                s_lastDiscordUpdate = now;
            }
        }

        [[nodiscard]] const char* getId() const override { return "Discord RPC"; }
    };

    REGISTER_HACK(DiscordRPC);

    class $modify(DiscordRPCGJBGLHook, GJBaseGameLayer) {
        bool init() override {
            if (!GJBaseGameLayer::init()) return false;
            s_levelTimestamp = std::time(nullptr);
            return true;
        }
    };
}
#endif
