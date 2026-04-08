#pragma once

// Zero-size marker component. When present on an entity, GameRegistry::view()
// automatically excludes it from query results. This is the ECS equivalent of
// Unity's activeInHierarchy == false.
struct DisabledTag {};
