#ifdef GEODE_IS_WINDOWS

#include "modules/bot/bot.hpp"
#include "modules/config/config.hpp"
#include "modules/hack/hack.hpp"
#include <memory>
#include <atomic>
#include <Zydis/Zydis.h>
#include <Windows.h>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>

using namespace geode::prelude;

namespace superhack::Bot {

static size_t getWriteSizeFromRIP(uint64_t rip, const CONTEXT* ctx) {
    uint8_t buffer[16];
    memcpy(buffer, (const void*)rip, sizeof(buffer));

    ZydisDecoder decoder;
    if (ZYAN_FAILED(ZydisDecoderInit(&decoder, ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_STACK_WIDTH_64))) {
        return 0;
    }

    ZydisDecodedInstruction insn;
    ZydisDecodedOperand ops[ZYDIS_MAX_OPERAND_COUNT];

    if (ZYAN_FAILED(ZydisDecoderDecodeFull(&decoder, buffer, sizeof(buffer), &insn, ops))) {
        return 0;
    }

    size_t writeSize = 0;
    const bool isRep = (insn.attributes & ZYDIS_ATTRIB_HAS_REP) != 0;

    if (isRep) {
        const ZydisMnemonic m = insn.mnemonic;
        const bool isMovs = (m == ZYDIS_MNEMONIC_MOVSB ||
                             m == ZYDIS_MNEMONIC_MOVSW ||
                             m == ZYDIS_MNEMONIC_MOVSD ||
                             m == ZYDIS_MNEMONIC_MOVSQ);

        const bool isStos = (m == ZYDIS_MNEMONIC_STOSB ||
                             m == ZYDIS_MNEMONIC_STOSW ||
                             m == ZYDIS_MNEMONIC_STOSD ||
                             m == ZYDIS_MNEMONIC_STOSQ);

        if (isMovs || isStos) {
            const ZyanU16 elementBits = ops[0].size;
            const ZyanU64 elementBytes = elementBits / 8;
            const ZyanU64 count = ctx->Rcx;
            return count * elementBytes;
        } else {
            return 0;
        }
    }

    for (uint8_t i = 0; i < insn.operand_count; ++i) {
        const auto& op = ops[i];
        if (op.type != ZYDIS_OPERAND_TYPE_MEMORY) continue;
        if (!(op.actions & ZYDIS_OPERAND_ACTION_WRITE)) continue;
        writeSize += (op.size / 8);
    }

    return writeSize;
}

std::string getLastWindowsErrorAsString() {
    DWORD errorMessageID = ::GetLastError();
    if(errorMessageID == 0) return std::string();

    LPSTR messageBuffer = nullptr;
    size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                                 NULL, errorMessageID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);

    std::string message(messageBuffer, size);
    LocalFree(messageBuffer);
    return message;
}

struct MemoryRestore { DWORD old; uintptr_t addr; };
struct MemoryEntry { uintptr_t addr; std::vector<uint8_t> data; size_t frame; };

class MemoryTracker {
public:
    MemoryTracker() {
        m_memoryLog.reserve(0x500000);
        SYSTEM_INFO si;
        GetSystemInfo(&si);
        m_pageSize = si.dwPageSize;
    }

    void enableCapture();
    void disableCapture();
    void addWatchedRegion(uintptr_t start, uintptr_t end);
    void clearWatchedRegions();
    void writeMemoryLog(size_t frame, uintptr_t addr, std::vector<uint8_t> data);
    void savePendingRestore(uintptr_t pageBase, DWORD oldProtect);
    bool consumePendingRestore(MemoryRestore& out);
    void restoreToFrame(size_t targetFrame);
    void clear();
    bool isInWatchedRegion(uintptr_t addr) const;
    bool isCaptureEnabled() const { return m_captureEnabled.load(std::memory_order_relaxed); }
    size_t getPageSize() const { return m_pageSize; }
    size_t getMemoryLogSize() const { return m_memoryLog.size(); }

private:
    std::atomic<bool> m_captureEnabled = false;
    std::mutex m_pendingMutex;
    std::mutex m_memoryLogMutex;
    std::vector<MemoryEntry> m_memoryLog;
    std::vector<MemoryRestore> m_memoryRestores;
    std::vector<MemoryRestore> m_pendingRestores;
    std::vector<std::pair<uintptr_t, uintptr_t>> m_memoryRegions;
    size_t m_pageSize = 0x1000;
};

static size_t s_currentFrame = 0;
static MemoryTracker s_memoryTracker;
static PVOID s_writeHandler = nullptr, s_stepHandler = nullptr;

LONG CALLBACK writeWatchHandler(PEXCEPTION_POINTERS info);
LONG CALLBACK singleStepHandler(PEXCEPTION_POINTERS info);

class $modify(MPFPlayLayer, PlayLayer) {
    static void onModify(auto& self);
    void pauseGame(bool p0);
    void loadFromCheckpoint(CheckpointObject* checkpoint);
    void resetLevel();
    void postUpdate(float dt);
    void togglePracticeMode(bool mode);
    void onQuit();
};

class $modify(GJBaseGameLayer) {
    void processCommands(float dt, bool isHalfTick, bool isLastTick) {
        GJBaseGameLayer::processCommands(dt, isHalfTick, isLastTick);
        s_currentFrame = m_gameState.m_currentProgress;
    }
};

$on_mod(Loaded) {
    s_writeHandler = AddVectoredExceptionHandler(1, writeWatchHandler);
    s_stepHandler = AddVectoredExceptionHandler(1, singleStepHandler);
}

}

#endif
