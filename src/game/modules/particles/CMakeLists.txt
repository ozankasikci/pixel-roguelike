add_library(game_module_particles STATIC
    ParticleModule.cpp
    ParticleSpawner.cpp
    ParticleSerializer.cpp
    ParticleSystem.cpp
)
target_include_directories(game_module_particles PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(game_module_particles PUBLIC
    engine_particles
    gameplay
)
