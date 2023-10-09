# set all cmake policies to its NEW behavior
cmake_minimum_required(VERSION 3.16)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

find_package(fuse3)
if (FUSE_FOUND)
    message(WARNING "GOT FUSE AND NOT AFRAID TO FUSE IT!")
else()
    message(FATAL_ERROR "NO FUSE FUSE USE SE E!!!")
endif()

set(CMAKE_CXX_STANDARD 17)

project(hello)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(${PROJECT_NAME}
    main.cpp
    ramfs.cpp
)

if (FUSE_FOUND)
  message(WARNING "USING FUSE, YAY!")
  
  target_compile_definitions(${PROJECT_NAME}
    PRIVATE HAS_FUSE=1
    PRIVATE _FILE_OFFSET_BITS=64
  )
  message("${FUSE_VERSION}")
  message("${FUSE_INCLUDE_DIRS}")
  message("${FUSE_LIBRARIES}")

else()
  message(WARNING "SADLY NO FUSE!!!!!!!!!!!")
endif()

target_include_directories(${PROJECT_NAME}
    PRIVATE ${FUSE_INCLUDE_DIR}
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE ${FUSE_LIBRARIES}
)
