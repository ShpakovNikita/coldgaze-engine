SET(PROJECT_NAME Coldgaze)
SET(PROJECT_FOLDER ${CMAKE_CURRENT_SOURCE_DIR}/framework)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/framework)

message(STATUS "Generating project file for ${PROJECT_FOLDER}")
# Main
file(GLOB_RECURSE HEADERS *.hpp *.h)
file(GLOB_RECURSE SOURCE ${HEADERS} *.cpp)

set(MAIN_CPP ${PROJECT_FOLDER}/main.cpp)
file(GLOB IMGUI_SOURCE "../include/imgui/*.cpp")

# Add shaders
set(SHADER_DIR "../data/shaders/${PROJECT_NAME}")
file(GLOB SHADERS "${SHADER_DIR}/*.vert" "${SHADER_DIR}/*.frag" "${SHADER_DIR}/*.comp" "${SHADER_DIR}/*.geom" "${SHADER_DIR}/*.tesc" "${SHADER_DIR}/*.tese" "${SHADER_DIR}/*.mesh" "${SHADER_DIR}/*.task" "${SHADER_DIR}/*.rgen" "${SHADER_DIR}/*.rchit" "${SHADER_DIR}/*.rmiss")
source_group("Shaders" FILES ${SHADERS})

if(MSVC)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS")
endif()

if(WIN32)
    set(COLDGAZE_LIBRARIES "")

    add_executable(${PROJECT_NAME} WIN32 ${MAIN_CPP} ${SOURCE} ${IMGUI_SOURCE} ${SHADERS})
    source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SOURCE})
    target_link_libraries(${PROJECT_NAME} ${Vulkan_LIBRARY} ${ASSIMP_LIBRARIES} ${WINLIBS} ${GLFW_LIBRARIES} ${COLDGAZE_LIBRARIES} ${SDL2_LIBRARY})
else(WIN32)
    add_executable(${PROJECT_NAME} ${MAIN_CPP} ${SOURCE} ${IMGUI_SOURCE} ${SHADERS} ${SDL2_LIBRARY})
    source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SOURCE})
    target_link_libraries(${PROJECT_NAME})
endif(WIN32)

set_target_properties(${PROJECT_NAME} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

if(RESOURCE_INSTALL_DIR)
    install(TARGETS ${PROJECT_NAME} DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()