#include "src/core/SkTDPQueue.h"
#include "src/utils/SkPolyUtils.h"
#include "checked_size.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <utility>
#include <vector>

namespace {
struct Entry {
    int priority = 0;
    int identity = 0;
    uint64_t payload = 0;
};
bool higher_priority(const Entry& a, const Entry& b) {
    const bool higher = a.priority < b.priority;
    return higher;
}
using Queue = SkTDPQueue<Entry, higher_priority>;
using Clock = std::chrono::steady_clock;

Entry make_entry(int identity, int priority) {
    Entry entry{priority, identity, UINT64_C(0x90000123456789ab) ^ static_cast<uint64_t>(identity)};
    return entry;
}
void hash_entry(uint64_t& digest, const Entry& entry) {
    digest ^= static_cast<uint32_t>(entry.identity);
    digest *= UINT64_C(1099511628211);
    digest ^= entry.payload;
    digest *= UINT64_C(1099511628211);
}
bool remove_minimum(Queue& queue, std::vector<Entry>& remaining, uint64_t& digest) {
    if (queue.count() != test_sizes::to_int(remaining.size()) || remaining.empty()) return false;
    int minimum = std::numeric_limits<int>::max();
    for (const Entry& entry : remaining) minimum = std::min(minimum, entry.priority);
    const Entry next = queue.peek();
    if (next.priority != minimum) return false;
    size_t position = 0;
    while (position < remaining.size() && remaining[position].identity != next.identity) ++position;
    if (position == remaining.size() || remaining[position].payload != next.payload) return false;
    remaining.erase(remaining.begin() + static_cast<std::ptrdiff_t>(position));
    hash_entry(digest, next);
    queue.pop();
    const bool countMatches = queue.count() == test_sizes::to_int(remaining.size());
    return countMatches;
}

bool permutations(uint64_t& digest, int& cases) {
    for (int count = 0; count <= 7; ++count) {
        std::vector<int> order(static_cast<size_t>(count));
        for (int index = 0; index < count; ++index) order[static_cast<size_t>(index)] = index;
        do {
            for (int divisor = 1; divisor <= 3; ++divisor) {
                Queue queue{};
                std::vector<Entry> remaining{};
                for (const int index : order) {
                    const Entry entry = make_entry(index, index / divisor);
                    queue.insert(entry);
                    remaining.push_back(entry);
                }
                while (!remaining.empty()) {
                    if (!remove_minimum(queue, remaining, digest)) return false;
                }
                if (queue.count() != 0) return false;
                ++cases;
            }
        } while (std::next_permutation(order.begin(), order.end()));
    }
    return true;
}

bool interleaved(uint64_t& digest) {
    for (int schedule = 1; schedule <= 64; ++schedule) {
        Queue queue(128);
        std::vector<Entry> remaining{};
        for (int step = 0; step < 300; ++step) {
            if (!remaining.empty() && (step * schedule) % 7 < 3) {
                if (!remove_minimum(queue, remaining, digest)) return false;
            } else {
                const int priority = (step * 13 + schedule * 17) % 31 - 15;
                const Entry entry = make_entry(step, priority);
                queue.insert(entry);
                remaining.push_back(entry);
            }
        }
        while (!remaining.empty()) {
            if (!remove_minimum(queue, remaining, digest)) return false;
        }
    }
    return true;
}

bool polygons(int& cases) {
    const std::array<SkPoint, 8> outline{{{0, 0}, {8, 0}, {8, 2}, {5, 2},
                                       {5, 5}, {2, 5}, {2, 8}, {0, 8}}};
    const std::array<SkPoint, 4> crossing{{{0, 0}, {8, 8}, {8, 0}, {0, 8}}};
    for (const float scale : std::array<float, 3>{0.5f, 1.0f, 16.0f}) {
        for (int winding = 0; winding < 2; ++winding) {
            for (int rotation = 0; rotation < 4; ++rotation) {
                for (int start = 0; start < 8; ++start) {
                    std::array<SkPoint, 8> points{};
                    for (int index = 0; index < 8; ++index) {
                        int source = (index + start) % 8;
                        if (winding != 0) source = 7 - source;
                        float x = outline[static_cast<size_t>(source)].fX;
                        float y = outline[static_cast<size_t>(source)].fY;
                        for (int turn = 0; turn < rotation; ++turn) {
                            const float oldX = x;
                            x = -y;
                            y = oldX;
                        }
                        points[static_cast<size_t>(index)].set(x * scale + 7.0f, y * scale - 3.0f);
                    }
                    if (!SkIsSimplePolygon(points.data(), 8)) return false;
                    ++cases;
                }
            }
        }
    }
    if (SkIsSimplePolygon(crossing.data(), 4)) return false;
    ++cases;
    return true;
}

#ifndef STX_SKIA_REFERENCE
template <typename T> concept TemporaryPeek = requires { T{}.peek(); };
static_assert(!TemporaryPeek<Queue>);
bool moved_queues() {
    Queue first{};
    first.insert(make_entry(1, 9));
    first.insert(make_entry(2, 3));
    Queue second(std::move(first));
    if (first.count() != 0 || second.peek().identity != 2) return false;
    first.insert(make_entry(3, 1));
    second = std::move(first);
    if (first.count() != 0 || second.count() != 1 || second.peek().identity != 3) return false;
    Queue& same = second;
    second = std::move(same);
    if (second.count() != 1 || second.peek().identity != 3) return false;
    return true;
}
#endif

int priority_for(int index, int count, int pattern) {
    switch (pattern) {
        case 0: return index;
        case 1: { const int priority = count - index; return priority; }
        case 2: return 1;
        case 3: { const int priority = (index * 919) % count; return priority; }
        default: return 0;
    }
}
void benchmark_polygons() {
    for (const int count : std::array<int, 3>{32, 256, 2048}) {
        std::vector<SkPoint> outline(static_cast<size_t>(count));
        outline[0].set(0.0f, 0.0f);
        outline[1].set(static_cast<float>(count - 3), 0.0f);
        for (int index = 2; index < count; ++index) {
            const int x = count - index - 1;
            outline[static_cast<size_t>(index)].set(static_cast<float>(x),
                                                  static_cast<float>(2 + x % 2));
        }
        const int rounds = 262144 / count;
        int accepted = 0;
        const Clock::time_point start = Clock::now();
        for (int round = 0; round < rounds; ++round) {
            const bool simple = SkIsSimplePolygon(outline.data(), count);
            if (simple) ++accepted;
        }
        const Clock::time_point end = Clock::now();
        const double seconds = std::chrono::duration<double>(end - start).count();
        SkASSERT_RELEASE(accepted == rounds);
        std::printf("polygon_%d,%d,%.9f,%d\n", count, count * rounds, seconds, accepted);
    }
}
void benchmark() {
    const std::array<const char*, 4> patterns{"ascending", "descending", "equal", "permuted"};
    for (const int count : std::array<int, 3>{32, 256, 2048}) {
        const int rounds = 262144 / count;
        for (int pattern = 0; pattern < 4; ++pattern) {
            std::vector<Entry> entries(static_cast<size_t>(count));
            for (int index = 0; index < count; ++index) {
                const int priority = priority_for(index, count, pattern);
                entries[static_cast<size_t>(index)] = make_entry(index, priority);
            }
            Queue queue(count);
            uint64_t digest = UINT64_C(1469598103934665603);
            const Clock::time_point start = Clock::now();
            for (int round = 0; round < rounds; ++round) {
                for (const Entry& entry : entries) queue.insert(entry);
                while (queue.count() != 0) {
                    hash_entry(digest, queue.peek());
                    queue.pop();
                }
            }
            const Clock::time_point end = Clock::now();
            const double seconds = std::chrono::duration<double>(end - start).count();
            std::printf("%s_%d,%d,%.9f,%llu\n", patterns[static_cast<size_t>(pattern)], count,
                        count * rounds, seconds, static_cast<unsigned long long>(digest));
        }
    }
}
}

int main(int argc, char** argv) {
    if (argc == 2 && std::strcmp(argv[1], "--benchmark") == 0) {
        benchmark();
        benchmark_polygons();
        return 0;
    }
    uint64_t digest = UINT64_C(1469598103934665603);
    int cases = 0;
    int polygonCases = 0;
    if (!permutations(digest, cases) || !interleaved(digest) || !polygons(polygonCases)) return 1;
#ifndef STX_SKIA_REFERENCE
    if (!moved_queues()) return 2;
#endif
    std::printf("priority queue: %d permutations, 64 mixed schedules, %d polygons, hash=%llu size=%zu\n",
                cases, polygonCases, static_cast<unsigned long long>(digest), sizeof(Queue));
    return 0;
}
