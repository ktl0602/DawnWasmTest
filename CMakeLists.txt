# CMakeList.txt: DawnWasmTest에 대한 CMake 프로젝트, 여기에 소스를 포함하고
# 프로젝트 특정 논리를 정의합니다.
#
cmake_minimum_required (VERSION 3.8)

# 지원되는 경우 MSVC 컴파일러에 대해 핫 다시 로드 사용하도록 설정합니다.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project ("DawnWasmTest")

set(INC_DIR "${CMAKE_CURRENT_LIST_DIR}/inc")
set(LIB_DIR "${CMAKE_CURRENT_LIST_DIR}/lib")
set(SRC_DIR "${CMAKE_CURRENT_LIST_DIR}/src")
set(EMS_DIR "${SRC_DIR}/platforms/ems")
set(IMGUI_DIR "${INC_DIR}/imgui")
set(DAWN_LIB_DIR "${LIB_DIR}/dawn/bin/win/x64/Debug")

set(SOURCES  "main.cpp" "${SRC_DIR}/Renderer.cpp" "${EMS_DIR}/RendererWindow.cpp"
	"${IMGUI_DIR}/imgui.cpp" "${IMGUI_DIR}/imgui_demo.cpp" "${IMGUI_DIR}/imgui_draw.cpp" "${IMGUI_DIR}/imgui_tables.cpp" "${IMGUI_DIR}/imgui_widgets.cpp"
	"${IMGUI_DIR}/imgui_impl_glfw.cpp" "${IMGUI_DIR}/imgui_impl_wgpu.cpp")

# 이 프로젝트의 실행 파일에 소스를 추가합니다.
add_executable (DawnWasmTest ${SOURCES})

# Add emscripten include directory
target_include_directories(DawnWasmTest PUBLIC "C:/MIDAS/Emscripten/emsdk/upstream/emscripten/cache/sysroot/include")
target_include_directories(DawnWasmTest PUBLIC ${INC_DIR})
target_include_directories(DawnWasmTest PUBLIC ${LIB_DIR}/dawn/inc)


add_library(DAWN_NATIVE_DLL "${DAWN_LIB_DIR}/dawn_native.dll")
add_library(DAWN_NATIVE_DLL_EXP "${DAWN_LIB_DIR}/dawn_native.dll.exp")
add_library(DAWN_NATIVE_DLL_LIB "${DAWN_LIB_DIR}/dawn_native.dll.lib")
add_library(DAWN_PLATFORM_DLL "${DAWN_LIB_DIR}/dawn_platform.dll")
add_library(DAWN_PLATFORM_DLL_EXP "${DAWN_LIB_DIR}/dawn_platform.dll.exp")
add_library(DAWN_PLATFORM_DLL_LIB "${DAWN_LIB_DIR}/dawn_platform.dll.lib")
add_library(DAWN_PROC_DLL "${DAWN_LIB_DIR}/dawn_proc.dll")
add_library(DAWN_PROC_DLL_EXP "${DAWN_LIB_DIR}/dawn_proc.dll.exp")
add_library(DAWN_PROC_DLL_LIB "${DAWN_LIB_DIR}/dawn_proc.dll.lib")
set_target_properties(DAWN_NATIVE_DLL PROPERTIES LINKER_LANGUAGE CXX)
set_target_properties(DAWN_NATIVE_DLL_EXP PROPERTIES LINKER_LANGUAGE CXX)
set_target_properties(DAWN_NATIVE_DLL_LIB PROPERTIES LINKER_LANGUAGE CXX)
set_target_properties(DAWN_PLATFORM_DLL PROPERTIES LINKER_LANGUAGE CXX)
set_target_properties(DAWN_PLATFORM_DLL_EXP PROPERTIES LINKER_LANGUAGE CXX)
set_target_properties(DAWN_PLATFORM_DLL_LIB PROPERTIES LINKER_LANGUAGE CXX)
set_target_properties(DAWN_PROC_DLL PROPERTIES LINKER_LANGUAGE CXX)
set_target_properties(DAWN_PROC_DLL_EXP PROPERTIES LINKER_LANGUAGE CXX)
set_target_properties(DAWN_PROC_DLL_LIB PROPERTIES LINKER_LANGUAGE CXX)

target_link_libraries(DawnWasmTest DAWN_NATIVE_DLL DAWN_NATIVE_DLL_EXP DAWN_NATIVE_DLL_LIB
DAWN_PLATFORM_DLL DAWN_PLATFORM_DLL_EXP DAWN_PLATFORM_DLL_LIB
DAWN_PROC_DLL DAWN_PROC_DLL_EXP DAWN_PROC_DLL_LIB) 

link_directories(${LIB_DIR})

macro(append_linker_flags FLAGS)
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${FLAGS}")
endmacro()

append_linker_flags("-Wall -Wformat -Os")
append_linker_flags("-s USE_GLFW=3 -s USE_WEBGPU=1 -s WASM=1")
append_linker_flags("-s ALLOW_MEMORY_GROWTH=1")
append_linker_flags("-s DISABLE_EXCEPTION_CATCHING=1 -s NO_EXIT_RUNTIME=0")
append_linker_flags("-s ASSERTIONS=1")
append_linker_flags("-s NO_FILESYSTEM=1 -DIMGUI_DISABLE_FILE_FUNCTIONS")
append_linker_flags("--bind")

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET DawnWasmTest PROPERTY CXX_STANDARD 20)
endif()

set(CMAKE_EXECUTABLE_SUFFIX .html)