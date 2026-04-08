#pragma once

// Tracks whether an entity was explicitly disabled (activeSelf) vs inherited
// from a disabled parent. Only added to entities that have been explicitly
// toggled at least once — absence implies activeSelf == true.
struct ActiveStateComponent {
    bool activeSelf = true;
};
