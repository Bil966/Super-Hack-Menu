#pragma once

#include <random>
#include <utility>
#include <modules/utils/SingletonCache.hpp>

enum class PlayerMode {
    Cube, Ship, Ball,
    UFO, Wave, Robot,
    Spider, Swing
};

namespace superhackmenu::gui {
    class Color;
}

namespace superhackmenu::utils {
    // Random number helpers (Geode v5)
    auto random(auto min, auto max) {
        return geode::utils::random::generate(min, max);
    }

    auto random(auto max) {
        return geode::utils::random::generate(0, max);
    }

    template <typename T>
    [[deprecated("Don't use MBO!")]] constexpr T& memberByOffset(void* ptr, size_t offset) {
        return *reinterpret_cast<T*>(reinterpret_cast<uintptr_t>(ptr) + offset);
    }

    std::string getClock(bool useTwelveHours = false);
    bool hasOpenGLExtension(std::string_view extension);
    bool shouldUseLegacyDraw();
    std::string formatTime(double time, bool showMillis = true);
    double getActualProgress(class GJBaseGameLayer* game);
    void updateCursorState(bool visible);
    std::string_view getMonthName(int month);

    using millis = std::chrono::milliseconds;
    using seconds = std::chrono::seconds;

    template <typename D = millis>
    time_t getTimestamp();

    gui::Color getRainbowColor(float speed, float saturation, float value, float offset = 0.f);
    PlayerMode getGameMode(class PlayerObject* player);
    std::string_view gameModeName(PlayerMode mode);
    int getPlayerIcon(PlayerMode mode);
    double getTPS();
    class cocos2d::CCMenu* getUILayer();

    bool matchesStringFuzzy(std::string_view haystack, std::string_view needle);

    template <typename T>
    std::span<T> paginate(std::vector<T> const& array, int size, int page) {
        if (size <= 0) return {};
        int startIndex = page * size;
        int endIndex = std::min(startIndex + size, static_cast<int>(array.size()));
        if (startIndex >= array.size()) return {};
        return {const_cast<T*>(array.data()) + startIndex, static_cast<size_t>(endIndex - startIndex)};
    }

    template <typename T>
    std::span<T> gradualPaginate(std::vector<T> const& array, int size, int page) {
        if (size <= 0) return {};
        if (page < 0) return {};
        int startIndex = page;
        int endIndex = std::min(startIndex + size, static_cast<int>(array.size()));
        if (startIndex >= array.size()) return {};
        return {const_cast<T*>(array.data()) + startIndex, static_cast<size_t>(endIndex - startIndex)};
    }

    size_t getBaseSize();
}
