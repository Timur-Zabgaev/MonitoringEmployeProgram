# CMakeList.txt: файл проекта CMake верхнего уровня; проведите глобальную конфигурацию
# и включите сюда подпроекты.
#
cmake_minimum_required (VERSION 3.8)

project ("client")

# Включите подпроекты.
#add_subdirectory ("client")

add_executable(main client/client.cpp "client/main.cpp")
target_link_libraries(main PRIVATE ws2_32)

