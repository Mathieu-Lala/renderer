add_executable(app main.cpp)

target_link_libraries(
  app PUBLIC project_options project_warnings CONAN_PKG::glm
             CONAN_PKG::imgui-sfml CONAN_PKG::spdlog)
