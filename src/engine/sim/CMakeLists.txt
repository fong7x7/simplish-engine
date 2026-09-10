# simplish-engine-sim — the deterministic tick, entity pools, tick hashing,
# and replay (Engine REQUIREMENTS §4, ADR-002, ADR-004)
#
# Public headers:  include/engine/sim/
# Implementation:  src/
#
# Game-agnostic: the game plugs its phases and state in through
# SimulationSystems. Nothing here reads a clock — time is the tick count.

simplish_add_module(simplish-engine-sim
    INCLUDE_DIR  ${CMAKE_CURRENT_SOURCE_DIR}/include
    SOURCES      ${CMAKE_CURRENT_SOURCE_DIR}/src/byte-reader.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/byte-writer.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/entity-slots.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/hash-divergence.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/input-queue.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/replay-codec.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/replay-recorder.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/replay-verification.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/simulation.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/state-hasher.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/tick-hash-builder.cpp
    DEPENDENCIES simplish-engine-math
                 simplish-engine-core
)

if(ENGINE_BUILD_TESTS)
    simplish_add_test(simplish-engine-sim-tests
        SOURCES      ${CMAKE_CURRENT_SOURCE_DIR}/test/support/scripted-input.cpp
                     ${CMAKE_CURRENT_SOURCE_DIR}/test/support/spark-world.cpp
                     ${CMAKE_CURRENT_SOURCE_DIR}/test/test_byte_reader.cpp
                     ${CMAKE_CURRENT_SOURCE_DIR}/test/test_determinism.cpp
                     ${CMAKE_CURRENT_SOURCE_DIR}/test/test_entity_slots.cpp
                     ${CMAKE_CURRENT_SOURCE_DIR}/test/test_hash_divergence.cpp
                     ${CMAKE_CURRENT_SOURCE_DIR}/test/test_input_queue.cpp
                     ${CMAKE_CURRENT_SOURCE_DIR}/test/test_replay_codec.cpp
                     ${CMAKE_CURRENT_SOURCE_DIR}/test/test_replay_recorder.cpp
                     ${CMAKE_CURRENT_SOURCE_DIR}/test/test_replay_verification.cpp
                     ${CMAKE_CURRENT_SOURCE_DIR}/test/test_simulation.cpp
                     ${CMAKE_CURRENT_SOURCE_DIR}/test/test_state_hasher.cpp
                     ${CMAKE_CURRENT_SOURCE_DIR}/test/test_tick_hash_builder.cpp
        DEPENDENCIES simplish-engine-sim
    )
    target_include_directories(simplish-engine-sim-tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/test)
endif()
