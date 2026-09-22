# ImGui is vendored as a submodule. It is built as its own target and
# deliberately does not link srnp_warnings: vendored code under -Wall -Wextra
# is noise we cannot act on.
set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../external/imgui)

add_library(imgui STATIC
  ${IMGUI_DIR}/imgui.cpp
  ${IMGUI_DIR}/imgui_draw.cpp
  ${IMGUI_DIR}/imgui_tables.cpp
  ${IMGUI_DIR}/imgui_widgets.cpp
  ${IMGUI_DIR}/imgui_demo.cpp
  ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
  ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp)

target_include_directories(imgui PUBLIC ${IMGUI_DIR} ${IMGUI_DIR}/backends)
target_link_libraries(imgui PUBLIC glfw OpenGL::GL)

add_executable(pairview
  main.cpp
  pair_view.cpp
  srnp_link.cpp
  format.cpp
  theme.cpp)
target_link_libraries(pairview PRIVATE srnp imgui srnp_warnings)

# The fonts are read from disk at startup: the installed copy first, then the
# source tree, so a build-tree binary works without installing anything.
set(SRNP_FONT_DIR_INSTALLED
    ${CMAKE_INSTALL_FULL_DATAROOTDIR}/srnp/fonts)
target_compile_definitions(pairview PRIVATE
  SRNP_FONT_DIR_INSTALLED="${SRNP_FONT_DIR_INSTALLED}"
  SRNP_FONT_DIR_SOURCE="${CMAKE_CURRENT_SOURCE_DIR}/fonts")

install(TARGETS pairview DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT srnp)
install(DIRECTORY fonts/ DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/srnp/fonts
        COMPONENT srnp FILES_MATCHING PATTERN "*.ttf" PATTERN "LICENSE-*"
        PATTERN "README.md")
