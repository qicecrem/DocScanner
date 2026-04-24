cmake_minimum_required(VERSION 3.19)
project(doc LANGUAGES CXX)


set(OpenCV_DIR "D:/ProgramData/OpenCV/huihut-OpenCV-MinGW-Build-0abb2b8/x64/mingw/lib")
find_package(OpenCV REQUIRED)
include_directories(D:/MSYS/mingw64/include)
link_directories(D:/MSYS/mingw64/lib)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets Concurrent Sql)



qt_standard_project_setup()

qt_add_executable(doc
    WIN32 MACOSX_BUNDLE
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    documentscanner.h documentscanner.cpp
    interactiveview.h interactiveview.cpp
    configmanager.h configmanager.cpp
    settingsdialog.h settingsdialog.cpp settingsdialog.ui

)

target_link_libraries(doc
    PRIVATE
        Qt::Core
        Qt::Widgets
        Qt6::Concurrent
        Qt6::Sql
        ${OpenCV_LIBS}
        tesseract
        leptonica
)


target_include_directories(doc PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${OpenCV_INCLUDE_DIRS})

install(TARGETS doc
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET doc
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
