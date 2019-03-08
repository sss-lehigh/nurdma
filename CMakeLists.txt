cmake_minimum_required(VERSION 3.0)
project(nuRDMA)

set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O2")

set(CMAKE_CXX_STANDARD 11)
find_package(Threads REQUIRED)

find_path(VERBS_INCLUDE_DIR infiniband/verbs.h)
find_library(VERBS_LIBRARIES ibverbs)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(verbs DEFAULT_MSG VERBS_LIBRARIES VERBS_INCLUDE_DIR)

if(VERBS_FOUND)
    include(CheckCXXSourceCompiles)
    CHECK_CXX_SOURCE_COMPILES("
    #include <infiniband/verbs.h>
    int main() {
      struct ibv_context* ctxt;
      struct ibv_exp_gid_attr gid_attr;
      ibv_exp_query_gid_attr(ctxt, 1, 0, &gid_attr);
      return 0;
    } " HAVE_IBV_EXP)
    if(NOT TARGET IBVerbs::verbs)
        add_library(IBVerbs::verbs UNKNOWN IMPORTED)
    endif()
    set_target_properties(IBVerbs::verbs PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${VERBS_INCLUDE_DIR}"
            IMPORTED_LINK_INTERFACE_LANGUAGES "C"
            IMPORTED_LOCATION "${VERBS_LIBRARIES}")
endif()

mark_as_advanced(
        VERBS_LIBRARIES
)

find_path(NUMA_INCLUDE_DIR numa.h)
find_library(NUMA_LIBRARIES numa)
find_package_handle_standard_args(Numa DEFAULT_MSG NUMA_LIBRARIES NUMA_INCLUDE_DIR)

if(NUMA_FOUND)
    if(NOT TARGET Numa::numa)
        add_library(Numa::numa UNKNOWN IMPORTED)
    endif()
    set_target_properties(Numa::numa PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${NUMA_INCLUDE_DIR}"
            IMPORTED_LINK_INTERFACE_LANGUAGES "C"
            IMPORTED_LOCATION "${NUMA_LIBRARIES}")
else()
    find_path(NUMA_INCLUDE_DIR numa.h)
    find_library(NUMA_LIBRARIES numactl-devel)
    find_package_handle_standard_args(Numa DEFAULT_MSG NUMA_LIBRARIES NUMA_INCLUDE_DIR)
    if(NUMA_FOUND)
        if(NOT TARGET Numa::numa)
            add_library(Numa::numa UNKNOWN IMPORTED)
        endif()
        set_target_properties(Numa::numa PROPERTIES
                INTERFACE_INCLUDE_DIRECTORIES "${NUMA_INCLUDE_DIR}"
                IMPORTED_LINK_INTERFACE_LANGUAGES "C"
                IMPORTED_LOCATION "${NUMA_LIBRARIES}")
    endif()
endif()

mark_as_advanced(NUMA_LIBRARIES)

include_directories("include")
add_executable(rc_bench src/rc_bench/main.cpp src/rc_bench/rc_resources.cpp src/utils/utils.cpp)
target_link_libraries(rc_bench Numa::numa IBVerbs::verbs Threads::Threads)

