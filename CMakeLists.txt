cmake_minimum_required(VERSION 3.21)
project(AutoArchero VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOUIC_SEARCH_PATHS ${CMAKE_SOURCE_DIR}/ui)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

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
    ui/mainwindow.ui
)

target_include_directories(AutoArchero PRIVATE ${CMAKE_SOURCE_DIR}/app)
target_link_libraries(AutoArchero PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets)

# Default paths baked for local BlueStacks + project venv (overridable via config.ini)
target_compile_definitions(AutoArchero PRIVATE
    AUTOARCHERO_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

if (WIN32)
    set_target_properties(AutoArchero PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

install(TARGETS AutoArchero RUNTIME DESTINATION bin)
