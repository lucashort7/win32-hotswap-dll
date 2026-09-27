cmake_minimum_required(VERSION 3.20)
project(win32-hotswap-dll C)

set(CMAKE_C_STANDARD 11)

add_executable(hotswap src/hotswap.c)
target_compile_options(hotswap PRIVATE -Wall -Wextra)
target_link_options(hotswap PRIVATE -static -s)

# test/probe.dll: attach/detach written to %TEMP%\hotswap-probe.log, for a round trip by hand
add_library(probe SHARED test/probe.c)
set_target_properties(probe PROPERTIES PREFIX "")
