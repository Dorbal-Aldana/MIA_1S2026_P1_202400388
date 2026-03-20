# Proyecto 1 MIA — backend (misma idea que CLASE7/CMakeLists.txt, con Server + Sesiones + Structs)
cmake_minimum_required(VERSION 3.10)
project(MIA_P1_Backend)

set(CMAKE_CXX_STANDARD 17)

add_executable(main
    main.cpp
    Server/Server.cpp
    Analyzer/Analyzer.cpp
    DiskManagement/DiskManagement.cpp
    Sessions/SessionManager.cpp
    Utilities/Utilities.cpp
    Structs/Structs.cpp
)

target_include_directories(main PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
