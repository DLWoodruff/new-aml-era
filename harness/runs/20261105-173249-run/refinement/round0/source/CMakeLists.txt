cmake_minimum_required(VERSION 3.18)
project(uc_solve C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
add_compile_options(-Wall -Wextra)

# Gurobi: $GUROBI_HOME if set, otherwise the install location of this project.
if(DEFINED ENV{GUROBI_HOME})
  set(GUROBI_HOME "$ENV{GUROBI_HOME}")
else()
  set(GUROBI_HOME "/home/dlw/software/gurobi1302/linux64")
endif()
find_path(GUROBI_INCLUDE_DIR gurobi_c.h HINTS "${GUROBI_HOME}/include" REQUIRED)
find_library(GUROBI_LIBRARY NAMES gurobi130 HINTS "${GUROBI_HOME}/lib" REQUIRED)
get_filename_component(GUROBI_LIB_DIR "${GUROBI_LIBRARY}" DIRECTORY)

# Instance handling needs no solver; the verifier and data checker use only this.
add_library(uc_data STATIC src/json.c src/instance.c)
target_include_directories(uc_data PUBLIC src)
target_link_libraries(uc_data PUBLIC m)

add_library(uc_model STATIC src/model.c)
target_include_directories(uc_model PUBLIC src ${GUROBI_INCLUDE_DIR})
target_link_libraries(uc_model PUBLIC uc_data ${GUROBI_LIBRARY})

add_executable(uc_solve src/main.c)
target_link_libraries(uc_solve PRIVATE uc_model)

# Checking tools (see NOTES.md). Neither is needed to solve an instance.
add_executable(uc_verify tools/verify_solution.c)
target_link_libraries(uc_verify PRIVATE uc_data)

add_executable(uc_unitcheck tools/unitcheck.c)
target_link_libraries(uc_unitcheck PRIVATE uc_model)

set_target_properties(uc_solve uc_unitcheck PROPERTIES
  BUILD_RPATH "${GUROBI_LIB_DIR}"
  INSTALL_RPATH "${GUROBI_LIB_DIR}")
