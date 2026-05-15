#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

enum class AssetWatchEventType : uint8_t
{
    Upsert = 0,
    Remove
};

struct AssetWatchEvent
{
    AssetWatchEventType type = AssetWatchEventType::Upsert;
    std::filesystem::path absolutePath;
};

class AssetWatcher
{
public:
    AssetWatcher() = default;
    ~AssetWatcher();

    bool Start(const std::filesystem::path& root, uint32_t pollIntervalMs = 1000);
    void Stop();

    std::vector<AssetWatchEvent> ConsumeEvents(size_t maxEvents = 512);

private:
    struct FileStamp
    {
        uint64_t writeTick = 0;
        uint64_t size = 0;
    };

    static uint64_t ToTick(const std::filesystem::file_time_type& ft);
    std::unordered_map<std::string, FileStamp> BuildSnapshot() const;
    void ThreadMain();

private:
    std::filesystem::path m_Root;
    uint32_t m_PollIntervalMs = 1000;

    std::atomic<bool> m_Running = false;
    std::thread m_Thread;
    std::condition_variable m_StopCv;
    std::mutex m_StopMutex;

    std::mutex m_QueueMutex;
    std::deque<AssetWatchEvent> m_Events;
    std::unordered_map<std::string, FileStamp> m_LastSnapshot;
};
