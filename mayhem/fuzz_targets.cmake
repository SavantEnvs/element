# Fuzz harness targets (fuzzer + -standalone) linked against the sanitized kv::element static lib.
# The element archive already holds the compiled JUCE modules (JUCE adds module sources to every
# consumer), so the harness executables link the archive file directly and take element's resolved
# compile settings, instead of linking kv::element and recompiling all of JUCE once per executable.
set(_mh "${CMAKE_SOURCE_DIR}/mayhem")
add_library(el_lsan_off OBJECT "${ELEMENT_LSAN_OFF}")
# Every harness links the Lua sandbox (mayhem/harnesses/lua_sandbox.c): the four luaopen_* wraps
# strip filesystem/process/native-code access from every Lua state Element opens.
add_library(el_lua_sandbox OBJECT "${_mh}/harnesses/lua_sandbox.c")
target_include_directories(el_lua_sandbox PRIVATE "${CMAKE_SOURCE_DIR}/src/lua/src")
set(_lua_wrap "LINKER:--wrap=luaopen_base,--wrap=luaopen_os,--wrap=luaopen_io,--wrap=luaopen_package")
add_library(el_standalone_main OBJECT "${ELEMENT_STANDALONE_MAIN}")
set_source_files_properties("${ELEMENT_STANDALONE_MAIN}" PROPERTIES LANGUAGE C)
separate_arguments(_fe NATIVE_COMMAND "${ELEMENT_FUZZ_ENGINE}")
find_package(PkgConfig REQUIRED)
pkg_check_modules(EL_FUZZ_SYS REQUIRED alsa freetype2 fontconfig)
foreach(_h IN LISTS ELEMENT_FUZZ_HARNESSES)
    foreach(_v IN ITEMS fuzz standalone)
        if(_v STREQUAL "fuzz")
            set(_t "${_h}")
        else()
            set(_t "${_h}-standalone")
        endif()
        add_executable(${_t} "${_mh}/harnesses/${_h}.cpp" $<TARGET_OBJECTS:el_lsan_off>
            $<TARGET_OBJECTS:el_lua_sandbox>)
        target_link_options(${_t} PRIVATE "${_lua_wrap}")
        add_dependencies(${_t} element)
        set_target_properties(${_t} PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
        target_include_directories(${_t} PRIVATE "${_mh}/harnesses" "${CMAKE_SOURCE_DIR}/src" ${GENERATED_INCLUDE_DIR}
            "$<TARGET_PROPERTY:element,INCLUDE_DIRECTORIES>")
        target_compile_definitions(${_t} PRIVATE "$<TARGET_PROPERTY:element,COMPILE_DEFINITIONS>")
        target_link_libraries(${_t} PRIVATE "$<TARGET_FILE:element>"
            element-binarydata element-luascripts element-luamods
            ${EL_FUZZ_SYS_LIBRARIES} dl pthread rt)
        if(_v STREQUAL "fuzz")
            target_link_options(${_t} PRIVATE ${_fe})
        else()
            target_sources(${_t} PRIVATE $<TARGET_OBJECTS:el_standalone_main>)
        endif()
    endforeach()
endforeach()
