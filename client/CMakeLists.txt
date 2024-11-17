# CMakeList.txt: файл проекта CMake верхнего уровня; проведите глобальную конфигурацию
# и включите сюда подпроекты.
#
cmake_minimum_required (VERSION 3.8)

project ("client")

# Включите подпроекты.
#add_subdirectory ("client")

add_executable(main client/client.cpp "client/main.cpp")
target_link_libraries(main PRIVATE ws2_32)
if(WIN32)
    add_custom_command(TARGET main POST_BUILD
        COMMAND reg ADD "HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Run" /v main /t REG_SZ /d \"${CMAKE_BINARY_DIR}/main.exe\" /f
        COMMENT "Registering application for auto-start on Windows"
    )
endif()


