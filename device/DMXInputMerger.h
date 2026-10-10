#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

// Owned exclusively by the receive thread. Times are monotonic milliseconds.
class DMXInputMerger
{
public:
    enum class Protocol { ArtNet, SACN };
    enum Mode { HTP, LTP };
    static constexpr int channelCount = 512;
    static constexpr double sacnTimeout = 2500;
    static constexpr double papWait = 1500;
    static constexpr double artnetTimeout = 10000;

    struct Output
    {
        int universe;
        std::array<uint8_t, channelCount> levels {};
        std::string sourceName;
    };

    explicit DMXInputMerger(Protocol p) : protocol(p) {}

    void clear() { universes.clear(); changeCounter = 0; }

    bool receive(int universe, const std::string& id, const uint8_t* data, int count,
                 double now, int priority = 100, uint8_t startCode = 0, int sequence = -1,
                 bool terminated = false, const std::string& name = {})
    {
        // Alternate start codes must never be interpreted as levels.
        if (!terminated && startCode != 0 && (protocol != Protocol::SACN || startCode != 0xdd)) return false;
        if (count < 0 || count > channelCount || (count > 0 && data == nullptr)
            || priority < 0 || priority > 200) return false;
        if (startCode == 0xdd && !terminated)
            for (int i = 0; i < count; ++i) if (data[i] > 200) return false;

        if (terminated)
        {
            auto u = universes.find(universe);
            if (u != universes.end() && u->second.sources.erase(id) != 0) u->second.dirty = true;
            return true;
        }

        auto& u = universes[universe];
        auto found = u.sources.find(id);
        if (found != u.sources.end())
        {
            const auto& s = found->second;
            const double lastData = s.hasLevels ? s.lastLevels : s.lastSeen;
            if (now - lastData >= timeout())
            {
                u.sources.erase(found);
                found = u.sources.end();
            }
        }
        const bool isNew = found == u.sources.end();
        if (!isNew && sequence >= 0 && found->second.sequence >= 0)
        {
            const int diff = sequence - found->second.sequence;
            if (protocol == Protocol::SACN)
            {
                // E1.31: one sequence stream per CID/universe, including PAP.
                if (diff <= 0 && diff > -20) return false;
            }
            else
            {
                const int advance = (diff + 256) % 256;
                if (advance == 0 || advance > 127) return false;
            }
        }

        auto& s = u.sources[id];
        s.sequence = sequence;
        s.lastSeen = now;
        s.name = name.empty() ? id : name;
        if (startCode == 0xdd)
        {
            s.priorities.fill(0); // Missing PAP slots are not sourced.
            std::copy_n(data, count, s.priorities.begin());
            s.hasPap = true;
            s.waitingForPap = false;
            s.lastPap = now;
        }
        else
        {
            if (!s.hasLevels)
            {
                s.firstLevels = now;
                s.waitingForPap = protocol == Protocol::SACN && !s.hasPap;
            }
            const auto change = ++changeCounter;
            for (int i = 0; i < channelCount; ++i)
            {
                const uint8_t level = i < count ? data[i] : uint8_t(0);
                if (i < count && (!s.hasLevels || i >= s.levelCount || level != s.levels[i]))
                    s.changed[i] = change;
                s.levels[i] = level;
            }
            s.levelCount = count;
            s.hasLevels = true;
            s.priority = priority;
            s.lastLevels = now;
        }
        u.dirty = true;
        return true;
    }

    // Poll even without incoming traffic so source/PAP loss and initial PAP waits expire.
    // Hold the last published look when every level source disappears.
    std::vector<Output> poll(double now, Mode mode)
    {
        std::vector<Output> output;
        for (auto u = universes.begin(); u != universes.end();)
        {
            auto& state = u->second;
            if (mode != lastMode) state.dirty = true;
            for (auto source = state.sources.begin(); source != state.sources.end();)
            {
                auto& s = source->second;
                if (now - (s.hasLevels ? s.lastLevels : s.lastSeen) >= timeout())
                {
                    source = state.sources.erase(source);
                    state.dirty = true;
                    continue;
                }
                if (s.hasPap && now - s.lastPap >= sacnTimeout)
                {
                    s.hasPap = false;
                    state.dirty = true;
                }
                if (s.waitingForPap && s.hasLevels && now - s.firstLevels >= papWait)
                {
                    s.waitingForPap = false;
                    state.dirty = true;
                }
                ++source;
            }
            if (state.sources.empty())
            {
                u = universes.erase(u);
                continue;
            }
            if (state.dirty)
            {
                Output merged { u->first };
                std::array<int, channelCount> priorities;
                priorities.fill(-1);
                std::array<uint64_t, channelCount> changed {};
                int active = 0;
                for (const auto& entry : state.sources)
                {
                    const auto& s = entry.second;
                    if (!s.hasLevels || s.waitingForPap) continue;
                    ++active;
                    merged.sourceName = s.name;
                    for (int i = 0; i < s.levelCount; ++i)
                    {
                        // PAP zero releases a slot; universe priority zero still
                        // sources it and maps to the lowest usable PAP (one).
                        const int p = s.hasPap ? s.priorities[i] : (std::max)(1, s.priority);
                        if (s.hasPap && p == 0) continue;
                        if (p > priorities[i] || (p == priorities[i]
                            && (mode == HTP ? s.levels[i] > merged.levels[i] : s.changed[i] > changed[i])))
                        {
                            priorities[i] = p;
                            merged.levels[i] = s.levels[i];
                            changed[i] = s.changed[i];
                        }
                    }
                }
                if (active > 1) merged.sourceName = "Merged (" + std::to_string(active) + " sources)";
                if (active > 0) output.push_back(std::move(merged));
                state.dirty = false;
            }
            ++u;
        }
        lastMode = mode;
        return output;
    }

private:
    struct Source
    {
        std::array<uint8_t, channelCount> levels {}, priorities {};
        std::array<uint64_t, channelCount> changed {};
        std::string name;
        double lastSeen = 0, lastLevels = 0, lastPap = 0, firstLevels = 0;
        int priority = 100, levelCount = 0, sequence = -1;
        bool hasLevels = false, hasPap = false, waitingForPap = false;
    };
    struct Universe
    {
        std::map<std::string, Source> sources;
        bool dirty = false;
    };
    double timeout() const { return protocol == Protocol::SACN ? sacnTimeout : artnetTimeout; }
    Protocol protocol;
    Mode lastMode = HTP;
    uint64_t changeCounter = 0;
    std::map<int, Universe> universes;
};
