SET(PROJECT_NAME Coldgaze)
SET(PROJECT_FOLDER ${CMAKE_CURRENT_SOURCE_DIR}/framework)
message(STATUS "Generating project file for ${PROJECT_FOLDER}")
# Main
file(GLOB SOURCE *.cpp ${BASE_HEADERS} ${PROJECT_FOLDER}/*.cpp)
SET(MAIN_CPP ${PROJECT_FOLDER}/${PROJECT_NAME}.cpp)
if(EXISTS ${PROJECT_FOLDER}/main.cpp)
    SET(MAIN_CPP ${PROJECT_FOLDER}/main.cpp)
ENDIF()
file(GLOB ADD_SOURCE "../include/imgui/*.cpp")
SET(SOURCE ${SOURCE} ${ADD_SOURCE})

# Add shaders
set(SHADER_DIR "../data/shaders/${PROJECT_NAME}")
file(GLOB SHADERS "${SHADER_DIR}/*.vert" "${SHADER_DIR}/*.frag" "${SHADER_DIR}/*.comp" "${SHADER_DIR}/*.geom" "${SHADER_DIR}/*.tesc" "${SHADER_DIR}/*.tese" "${SHADER_DIR}/*.mesh" "${SHADER_DIR}/*.task" "${SHADER_DIR}/*.rgen" "${SHADER_DIR}/*.rchit" "${SHADER_DIR}/*.rmiss")
source_group("Shaders" FILES ${SHADERS})

if(WIN32)
    set(COLDGAZE_LIBRARIES glfw)

    add_executable(${PROJECT_NAME} WIN32 ${MAIN_CPP} ${SOURCE} ${SHADERS})
    target_link_libraries(${PROJECT_NAME} ${Vulkan_LIBRARY} ${ASSIMP_LIBRARIES} ${WINLIBS} ${GLFW_LIBRARIES} ${COLDGAZE_LIBRARIES})
else(WIN32)
    add_executable(${PROJECT_NAME} ${MAIN_CPP} ${SOURCE} ${SHADERS})
    target_link_libraries(${PROJECT_NAME} ${GLFW_LIBRARIES})
endif(WIN32)

set_target_properties(${PROJECT_NAME} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

if(RESOURCE_INSTALL_DIR)
    install(TARGETS ${PROJECT_NAME} DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()