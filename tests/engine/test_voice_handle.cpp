#include <cassert>
#include <cstdio>
#include <unordered_set>

#include "engine/audio/VoiceHandle.h"

using engine::audio::VoiceHandle;

int main() {
    // Default handle is invalid
    {
        VoiceHandle h;
        assert(!h.valid());
        assert(h.id == 0);
        assert(h.generation == 0);
    }

    // Constructed handle is valid
    {
        VoiceHandle h(1);
        assert(h.valid());
        assert(h.id == 1);
        assert(h.generation == 0);

        VoiceHandle h2(5, 3);
        assert(h2.valid());
        assert(h2.id == 5);
        assert(h2.generation == 3);
    }

    // Equality and inequality
    {
        VoiceHandle a(1, 0);
        VoiceHandle b(1, 0);
        VoiceHandle c(2, 0);
        VoiceHandle d(1, 1);

        assert(a == b);
        assert(a != c);
        assert(a != d);
    }

    // Usable as unordered_set key
    {
        std::unordered_set<VoiceHandle> set;
        set.insert(VoiceHandle(1, 0));
        set.insert(VoiceHandle(2, 0));
        set.insert(VoiceHandle(1, 0)); // duplicate

        assert(set.size() == 2);
        assert(set.count(VoiceHandle(1, 0)) == 1);
        assert(set.count(VoiceHandle(2, 0)) == 1);
        assert(set.count(VoiceHandle(3, 0)) == 0);
    }

    // Generation distinguishes reused IDs
    {
        VoiceHandle gen0(1, 0);
        VoiceHandle gen1(1, 1);
        VoiceHandle gen2(1, 2);

        assert(gen0 != gen1);
        assert(gen1 != gen2);
        assert(gen0 != gen2);

        std::unordered_set<VoiceHandle> set;
        set.insert(gen0);
        set.insert(gen1);
        set.insert(gen2);
        assert(set.size() == 3);
    }

    std::printf("test_voice_handle: all assertions passed\n");
    return 0;
}
