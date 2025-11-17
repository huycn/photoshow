cmake_minimum_required(VERSION 3.16)
project(SimpleSlideShow LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets OpenGLWidgets)

add_executable(sss
    main.cpp
    imagewidget.cpp
)

target_link_libraries(sss
    Qt6::Widgets
    Qt6::OpenGLWidgets
)
