#include "AssetWatcher.h"

#include <chrono>

AssetWatcher::~AssetWatcher()
{
    Stop();
}

bool AssetWatcher::Start(const std::filesystem::path& root, uint32_t pollIntervalMs)
{
    Stop();

    m_Root = root;
    m_PollIntervalMs = (pollIntervalMs == 0) ? 250 : pollIntervalMs;

    std::error_code ec;
    if (!std::filesystem::exists(m_Root, ec) || ec)
    {
        return false;
    }

    m_LastSnapshot = BuildSnapshot();
    m_Running = true;
    m_Thread = std::thread(&AssetWatcher::ThreadMain, this);
    return true;
}

void AssetWatcher::Stop()
{
    if (!m_Running.exchange(false))
    {
        return;
    }

    m_StopCv.notify_all();
    if (m_Thread.joinable())
    {
        m_Thread.join();
    }
}

std::vector<AssetWatchEvent> AssetWatcher::ConsumeEvents(size_t maxEvents)
{
    std::vector<AssetWatchEvent> out;
    out.reserve(maxEvents);

    std::lock_guard<std::mutex> lock(m_QueueMutex);
    while (!m_Events.empty() && out.size() < maxEvents)
    {
        out.emplace_back(std::move(m_Events.front()));
        m_Events.pop_front();
    }
    return out;
}

uint64_t AssetWatcher::ToTick(const std::filesystem::file_time_type& ft)
{
    const auto ns = std::chrono::time_point_cast<std::chrono::nanoseconds>(ft).time_since_epoch().count();
    return (uint64_t)((ns < 0) ? 0 : ns);
}

std::unordered_map<std::string, AssetWatcher::FileStamp> AssetWatcher::BuildSnapshot() const
{
    std::unordered_map<std::string, FileStamp> snapshot;

    std::error_code ec;
    if (!std::filesystem::exists(m_Root, ec) || ec)
    {
        return snapshot;
    }

    for (std::filesystem::recursive_directory_iterator it(m_Root, ec), end; it != end && !ec; it.increment(ec))
    {
        if (ec)
        {
            break;
        }

        const std::filesystem::directory_entry& e = *it;
        if (!e.is_regular_file(ec) || ec)
        {
            continue;
        }

        const std::filesystem::path path = e.path().lexically_normal();
        const std::string key = path.string();

        FileStamp stamp{};
        stamp.writeTick = ToTick(e.last_write_time(ec));
        if (ec)
        {
            ec.clear();
            continue;
        }

        stamp.size = (uint64_t)e.file_size(ec);
        if (ec)
        {
            ec.clear();
            continue;
        }

        snapshot.emplace(key, stamp);
    }

    return snapshot;
}

void AssetWatcher::ThreadMain()
{
    while (m_Running)
    {
        const auto current = BuildSnapshot();

        {
            std::lock_guard<std::mutex> qLock(m_QueueMutex);

            for (const auto& kv : current)
            {
                const auto itPrev = m_LastSnapshot.find(kv.first);
                const bool isNew = (itPrev == m_LastSnapshot.end());
                const bool isChanged = !isNew &&
                    (itPrev->second.writeTick != kv.second.writeTick || itPrev->second.size != kv.second.size);
                if (isNew || isChanged)
                {
                    AssetWatchEvent ev{};
                    ev.type = AssetWatchEventType::Upsert;
                    ev.absolutePath = std::filesystem::path(kv.first);
                    m_Events.emplace_back(std::move(ev));
                }
            }

            for (const auto& kv : m_LastSnapshot)
            {
                if (current.find(kv.first) != current.end())
                {
                    continue;
                }

                AssetWatchEvent ev{};
                ev.type = AssetWatchEventType::Remove;
                ev.absolutePath = std::filesystem::path(kv.first);
                m_Events.emplace_back(std::move(ev));
            }
        }

        m_LastSnapshot = current;

        std::unique_lock<std::mutex> stopLock(m_StopMutex);
        m_StopCv.wait_for(stopLock, std::chrono::milliseconds(m_PollIntervalMs), [this]()
        {
            return !m_Running.load();
        });
    }
}
