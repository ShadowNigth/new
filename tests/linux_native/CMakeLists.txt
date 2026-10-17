cmake_minimum_required(VERSION 3.20)
project(linux_native_runtime_test C)
set(CMAKE_C_STANDARD 11)
get_filename_component(BLACK_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
set(XBOXRECOMP_DIR "${BLACK_ROOT}/third_party/xboxrecomp" CACHE PATH "Patched runtime")
set(DSP56300_SOURCE_DIR "${BLACK_ROOT}/.work/err009-dsp56300" CACHE PATH "Pinned DSP core")
add_subdirectory("${XBOXRECOMP_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/xboxrecomp")
add_executable(linux-native-runtime-test test_main.c)
target_link_libraries(linux-native-runtime-test PRIVATE
    platform xbox_video xbox_input xbox_apu)
enable_testing()
add_test(NAME linux_native_runtime COMMAND linux-native-runtime-test)
set_tests_properties(linux_native_runtime PROPERTIES
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy;RECOMP_FB_WINDOW=1")
