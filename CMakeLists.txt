cmake_minimum_required(VERSION 3.21)
project(AutoArchero VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOUIC_SEARCH_PATHS ${CMAKE_SOURCE_DIR}/ui)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Svg OpenGL Concurrent)

# QtNodes (paceholder/nodeeditor) — éditeur de graphe Workflow
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(USE_QT6 ON CACHE BOOL "" FORCE)
add_subdirectory(third_party/nodeeditor EXCLUDE_FROM_ALL)

qt_add_executable(AutoArchero
    app/main.cpp
    app/MainWindow.cpp
    app/MainWindow.h
    app/ImageViewer.cpp
    app/ImageViewer.h
    app/AdbClient.cpp
    app/AdbClient.h
    app/OcrBridge.cpp
    app/OcrBridge.h
    app/CaptureWorker.cpp
    app/CaptureWorker.h
    app/TemplateStore.cpp
    app/TemplateStore.h
    app/RoiStore.cpp
    app/RoiStore.h
    app/Types.h
    app/WorkflowEditor.cpp
    app/WorkflowEditor.h
    app/WorkflowRunner.cpp
    app/WorkflowRunner.h
    app/nodes/PlaceholderNode.cpp
    app/nodes/PlaceholderNode.h
    app/nodes/FlowData.h
    app/nodes/WorkflowData.h
    app/nodes/WorkflowContext.h
    app/nodes/TapNode.cpp
    app/nodes/TapNode.h
    app/nodes/WaitNode.cpp
    app/nodes/WaitNode.h
    app/nodes/CaptureNode.cpp
    app/nodes/CaptureNode.h
    app/nodes/CheckRoiTemplatesNode.cpp
    app/nodes/CheckRoiTemplatesNode.h
    app/nodes/IfTemplateNode.cpp
    app/nodes/IfTemplateNode.h
    app/nodes/IfRoiNode.cpp
    app/nodes/IfRoiNode.h
    app/nodes/IfRoiRegexNode.cpp
    app/nodes/IfRoiRegexNode.h
    app/nodes/CombatNode.cpp
    app/nodes/CombatNode.h
    app/nodes/WhileNode.cpp
    app/nodes/WhileNode.h
    app/icons.qrc
    ui/mainwindow.ui
)

target_include_directories(AutoArchero PRIVATE
    ${CMAKE_SOURCE_DIR}/app
    ${CMAKE_SOURCE_DIR}/app/nodes
)
target_link_libraries(AutoArchero PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Svg
    Qt6::OpenGL
    Qt6::Concurrent
    QtNodes::QtNodes
)

# Default paths baked for local BlueStacks + project venv (overridable via config.ini)
target_compile_definitions(AutoArchero PRIVATE
    AUTOARCHERO_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

if (WIN32)
    set_target_properties(AutoArchero PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

install(TARGETS AutoArchero RUNTIME DESTINATION bin)
