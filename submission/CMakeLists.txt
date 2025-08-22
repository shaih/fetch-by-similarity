cmake_minimum_required(VERSION 3.14)
project(add_openfhe LANGUAGES CXX)

# --------------------------------------------------------------------
# 1.  Compiler / flags
# --------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
# set(CMAKE_POSITION_INDEPENDENT_CODE ON)
# add_compile_options(-O3 -Wall -Wextra -Wno-unused-parameter)
option( BUILD_STATIC "Set to ON to include static versions of the library" OFF)

# --------------------------------------------------------------------
# 2.  Find the OpenFHE *package* that the install script put into
#     third_party/openfhe
# --------------------------------------------------------------------
#   scripts/get-openfhe.sh installs:
#     third_party/openfhe/lib/cmake/OpenFHE/OpenFHEConfig.cmake

find_package(OpenFHE CONFIG REQUIRED)
if (OpenFHE_FOUND)
    message(STATUS "FOUND PACKAGE OpenFHE")
    message(STATUS "OpenFHE Version: ${BASE_OPENFHE_VERSION}")
    message(STATUS "OpenFHE installed as shared libraries: ${OpenFHE_SHARED}")
    message(STATUS "OpenFHE include files location: ${OpenFHE_INCLUDE}")
    message(STATUS "OpenFHE lib files location: ${OpenFHE_LIBDIR}")
    message(STATUS "OpenFHE Native Backend size: ${OpenFHE_NATIVE_SIZE}")
else()
    message(FATAL_ERROR "PACKAGE OpenFHE NOT FOUND")
endif ()

set( CMAKE_CXX_FLAGS "${OpenFHE_CXX_FLAGS} -Werror")


# --------------------------------------------------------------------
# 3.  Link libraries
# --------------------------------------------------------------------

include_directories( ${OPENMP_INCLUDES} )
include_directories( ${OpenFHE_INCLUDE} )
include_directories( ${OpenFHE_INCLUDE}/third-party/include )
include_directories( ${OpenFHE_INCLUDE}/core )
include_directories( ${OpenFHE_INCLUDE}/pke )
include_directories( ${OpenFHE_INCLUDE}/binfhe )
### add directories for other OpenFHE modules as needed for your project

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

### add link options to correctly pass params to invoked linker program
### this should eliminate the need for setting LD_LIRARY_PATH
if(NOT APPLE)
    add_link_options(-Wl,--no-as-needed)
endif()

### Set link directories for third party library requirements
link_directories( ${OpenFHE_LIBDIR} )
link_directories( ${OPENMP_LIBRARIES} )


if(BUILD_STATIC)
    set( CMAKE_EXE_LINKER_FLAGS "${OpenFHE_EXE_LINKER_FLAGS} -static")
    link_libraries( ${OpenFHE_STATIC_LIBRARIES} )
    message(STATUS "Build with static libs: ${OpenFHE_STATIC_LIBRARIES}")
else()
    set( CMAKE_EXE_LINKER_FLAGS ${OpenFHE_EXE_LINKER_FLAGS} )
    link_libraries( ${OpenFHE_SHARED_LIBRARIES} )
    message(STATUS "Build with shared libs: ${OpenFHE_SHARED_LIBRARIES}")
endif()

# --------------------------------------------------------------------
# 4.  Each *.cpp file becomes its own executable.
#     The eight stage names are hard-wired by the benchmark contract.
# --------------------------------------------------------------------
 
add_executable( client_key_generation src/running_sums.cpp src/slot_replication.cpp src/client_key_generation.cpp )
# target_include_directories(client_key_generation PRIVATE include)

add_executable( client_preprocess_dataset src/client_preprocess_dataset.cpp )
# target_include_directories(client_preprocess PRIVATE include)

add_executable( client_encode_encrypt_db src/client_encode_encrypt_db.cpp )
# target_include_directories(client_encode_encrypt_db PRIVATE include)

add_executable( client_encode_encrypt_query src/client_encode_encrypt_query.cpp )
# target_include_directories(client_encode_encrypt_query PRIVATE include)

add_executable( client_decrypt_decode src/client_decrypt_decode.cpp )
# target_include_directories(client_decode_decrypt PRIVATE include)

add_executable( client_postprocess src/running_sums.cpp src/client_postprocess.cpp )
# target_include_directories(client_postprocess PRIVATE include)

add_executable( server_preprocess_dataset src/server_preprocess_dataset.cpp )
# target_include_directories(server_preprocess PRIVATE include)

add_executable( server_encrypted_compute src/running_sums.cpp src/slot_replication.cpp src/server_encrypted_compute.cpp )
# target_include_directories(server_encrypted_compute PRIVATE include)
