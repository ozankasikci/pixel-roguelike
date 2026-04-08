#include "engine/ecs/GameRegistry.h"
#include "engine/ecs/DisabledTag.h"

#include <cassert>

struct Position { float x = 0.0f; };
struct Velocity { float v = 0.0f; };

int main() {
    // --- view() excludes DisabledTag ---
    {
        GameRegistry reg;
        auto e1 = reg.create();
        auto e2 = reg.create();
        reg.emplace<Position>(e1, Position{1.0f});
        reg.emplace<Position>(e2, Position{2.0f});
        reg.emplace<DisabledTag>(e2);

        int count = 0;
        for (auto [entity, pos] : reg.view<Position>().each()) {
            assert(entity == e1);
            count++;
        }
        assert(count == 1);
    }

    // --- viewAll() includes DisabledTag ---
    {
        GameRegistry reg;
        auto e1 = reg.create();
        auto e2 = reg.create();
        reg.emplace<Position>(e1, Position{1.0f});
        reg.emplace<Position>(e2, Position{2.0f});
        reg.emplace<DisabledTag>(e2);

        int count = 0;
        for (auto entity : reg.viewAll<Position>()) {
            (void)entity;
            count++;
        }
        assert(count == 2);
    }

    // --- view() with multiple components ---
    {
        GameRegistry reg;
        auto e1 = reg.create();
        auto e2 = reg.create();
        reg.emplace<Position>(e1);
        reg.emplace<Velocity>(e1);
        reg.emplace<Position>(e2);
        reg.emplace<Velocity>(e2);
        reg.emplace<DisabledTag>(e2);

        int count = 0;
        for (auto entity : reg.view<Position, Velocity>()) {
            assert(entity == e1);
            count++;
        }
        assert(count == 1);
    }

    // --- emplace, get, try_get, valid, all_of, destroy ---
    {
        GameRegistry reg;
        auto e = reg.create();
        reg.emplace<Position>(e, Position{42.0f});

        assert(reg.valid(e));
        assert(reg.all_of<Position>(e));
        assert(reg.get<Position>(e).x == 42.0f);
        assert(reg.try_get<Position>(e) != nullptr);
        assert(reg.try_get<Velocity>(e) == nullptr);

        reg.destroy(e);
        assert(!reg.valid(e));
    }

    return 0;
}
