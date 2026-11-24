# Injected with -DCMAKE_PROJECT_INCLUDE by mayhem/build.sh (no upstream file is edited). Defers adding
# the fuzz targets until the top-level CMakeLists.txt has defined kv::element. CMake includes this
# after every project() call (sub-projects too), so only the first, top-level one registers.
get_property(_el_fuzz_done GLOBAL PROPERTY ELEMENT_FUZZ_DEFERRED)
if(ELEMENT_FUZZ_HARNESSES AND NOT _el_fuzz_done)
    set_property(GLOBAL PROPERTY ELEMENT_FUZZ_DEFERRED ON)
    cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}"
        CALL include "${CMAKE_SOURCE_DIR}/mayhem/fuzz_targets.cmake")
endif()
