cmake_minimum_required(VERSION 3.20)
project(black_xbox_recomp C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
add_compile_definitions(WIN32_LEAN_AND_MEAN NOMINMAX)

set(XBOXRECOMP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/xboxrecomp" CACHE PATH
    "Path to the patched XboxRecomp toolkit")

# Release symbols let the crash handler identify the original guest function.
if(MSVC)
    string(APPEND CMAKE_C_FLAGS_RELEASE " /Zi")
    string(APPEND CMAKE_CXX_FLAGS_RELEASE " /Zi")
    string(APPEND CMAKE_EXE_LINKER_FLAGS_RELEASE " /DEBUG /OPT:REF /OPT:ICF")
elseif(UNIX)
    # Same job as the PDB on Windows: -g names the faulting generated
    # function via dladdr/addr2line in crash reports.
    string(APPEND CMAKE_C_FLAGS_RELEASE " -g")
endif()

add_subdirectory("${XBOXRECOMP_DIR}" "${CMAKE_BINARY_DIR}/xboxrecomp")
file(GLOB RECOMP_GEN_SOURCES CONFIGURE_DEPENDS "src/recomp/gen/*.c")
if(NOT RECOMP_GEN_SOURCES)
    message(FATAL_ERROR "Supply your retail BLACK Xbox files, then run scripts/setup.ps1 on Windows, scripts/setup-linux.sh for the Wine build, or scripts/setup-native-linux.sh for the native Linux build.")
endif()

if(WIN32)
    add_executable(${PROJECT_NAME} WIN32
        src/main.c
        src/recomp_manual.c
        ${RECOMP_GEN_SOURCES}
    )
else()
    add_executable(${PROJECT_NAME}
        src/main.c
        src/recomp_manual.c
        ${RECOMP_GEN_SOURCES}
    )
endif()
option(RECOMP_ICALL_FEEDBACK "Record indirect call targets for the next generation pass" OFF)
option(RECOMP_TRACE_UNRESOLVED_STUBS "Log unresolved guest call targets and state" OFF)
if(RECOMP_ICALL_FEEDBACK)
    target_compile_definitions(xbox_kernel PRIVATE
        RECOMP_ICALL_FEEDBACK
        RECOMP_ICALL_FEEDBACK_PATH=\"local/icall_targets.dump\")
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        RECOMP_ICALL_FEEDBACK
        RECOMP_ICALL_FEEDBACK_PATH=\"local/icall_targets.dump\")
endif()
if(RECOMP_TRACE_UNRESOLVED_STUBS)
    target_compile_definitions(${PROJECT_NAME} PRIVATE RECOMP_TRACE_UNRESOLVED_STUBS)
endif()
target_include_directories(${PROJECT_NAME} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/recomp"
    "${XBOXRECOMP_DIR}/include"
)
if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        xboxrecomp d3d11 dxgi dxguid xinput winmm dbghelp
    )
else()
    # Native Linux: the patched runtime carries its own backends
    # (Kelvin CPU renderer + SDL2 presentation/audio/input, POSIX kernel
    # layer). Only the dynamic loader and libm are host libraries here;
    # SDL2/OpenSSL/epoxy come in through the runtime targets.
    target_link_libraries(${PROJECT_NAME} PRIVATE xboxrecomp dl m)
    target_link_options(${PROJECT_NAME} PRIVATE -rdynamic)
endif()
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /bigobj)
    target_compile_definitions(${PROJECT_NAME} PRIVATE _CRT_SECURE_NO_WARNINGS)
    if(CMAKE_C_COMPILER_ID STREQUAL "MSVC")
        target_compile_options(${PROJECT_NAME} PRIVATE /MP)
    endif()
    target_link_options(${PROJECT_NAME} PRIVATE /LARGEADDRESSAWARE)
elseif(UNIX)
    # The Kelvin renderer and the generated code use AVX2 intrinsics;
    # the native Linux build keeps the Windows build's AVX2 requirement.
    include(CheckCCompilerFlag)
    check_c_compiler_flag("-mavx2" BLACK_HAS_AVX2_FLAG)
    if(BLACK_HAS_AVX2_FLAG)
        target_compile_options(${PROJECT_NAME} PRIVATE -mavx2)
    endif()
endif()
