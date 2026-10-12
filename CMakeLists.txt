cmake_minimum_required(VERSION 3.25.1)
project(cppgram VERSION 0.1.23 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_compile_options(-O2 -Wall -Wextra)

find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)

add_library(cppgram STATIC
    src/client.cpp
    src/session.cpp
    src/calls.cpp
    src/messages.cpp
    src/schema.cpp
    src/api_methods.cpp
    src/updates.cpp
    src/handlers.cpp
    src/keygen.cpp
    src/format.cpp
    src/update_dispatch.cpp
    src/wrappers.cpp
    src/parity_wrappers.cpp
    src/filters.cpp
    src/utils.cpp
    src/html.cpp
    src/markdown.cpp
    src/parser.cpp
)
target_include_directories(cppgram PUBLIC include)
target_link_libraries(cppgram PUBLIC OpenSSL::Crypto ZLIB::ZLIB)

add_executable(cg_tool tools/cg_tool.cpp)
target_link_libraries(cg_tool PRIVATE cppgram)

# Parser-layer regression corpus (expectations captured from the kurigram
# reference via scripts/gen_parity_test.py).
enable_testing()
add_executable(cg_parser_parity tests/parser_parity.cpp)
target_link_libraries(cg_parser_parity PRIVATE cppgram)
add_test(NAME parser_parity COMMAND cg_parser_parity)

# Malformed-JSON rejection and json.loads output parity for the schema reader.
add_executable(cg_json_strict tests/json_strict.cpp)
target_link_libraries(cg_json_strict PRIVATE cppgram)
add_test(NAME json_strict COMMAND cg_json_strict)

# Session string codecs: gogram json/legacy + pyrogram, round-tripped.
add_executable(cg_session_roundtrip tests/session_roundtrip.cpp)
target_link_libraries(cg_session_roundtrip PRIVATE cppgram)
add_test(NAME session_roundtrip COMMAND cg_session_roundtrip)

# High-traffic wrappers must still encode to the layer-230 shape; CG_DRY_RUN
# makes this offline so it does not depend on a live account.
add_executable(cg_wire_shape tests/wire_shape.cpp)
target_link_libraries(cg_wire_shape PRIVATE cppgram)
add_test(NAME wire_shape COMMAND cg_wire_shape)

# SRP: the path that carries a cloud password. A null BN_CTX segfaulted it and
# A/M1 were sent as base64url text, both found only by logging in live.
add_executable(cg_srp tests/srp_shape.cpp)
target_link_libraries(cg_srp PRIVATE cppgram)
add_test(NAME srp_shape COMMAND cg_srp)
