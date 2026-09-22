/*
  main.cpp - The PairView executable: window, render loop, teardown.

  Copyright (C) 2015  Chittaranjan Srinivas Swaminathan

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>
*/

#include "pair_view.h"
#include "srnp_link.h"
#include "theme.h"

#include <srnp/srnp_print.h>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <GLFW/glfw3.h>

#include <charconv>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>

namespace {

/// Redraws on input, and otherwise ten times a second. Enough for watching
/// a blackboard, and close to free when nothing is happening.
constexpr double kFrameTimeout = 0.1;

struct Options {
  std::string master_ip = "127.0.0.1";
  std::string master_port = "12321";
  int owner_id = srnp::kAnyOwner;
};

void printUsage() {
  std::puts(
      "pairview - a viewer and editor for the srnp pair space\n"
      "\n"
      "Usage: pairview [--master-ip IP] [--master-port PORT] [--owner-id N]\n"
      "\n"
      "The master's address defaults to SRNP_MASTER_IP and SRNP_MASTER_PORT,\n"
      "then to 127.0.0.1:12321.");
}

/// Returns nullopt when the arguments do not make sense, having said why.
std::optional<Options> parseArguments(int argc, char* argv[]) {
  Options options;
  if (const char* ip = std::getenv("SRNP_MASTER_IP")) options.master_ip = ip;
  if (const char* port = std::getenv("SRNP_MASTER_PORT")) options.master_port = port;

  for (int i = 1; i < argc; ++i) {
    const std::string_view argument(argv[i]);

    if (argument == "--help" || argument == "-h") {
      printUsage();
      return std::nullopt;
    }

    if (i + 1 >= argc) {
      std::fprintf(stderr, "%s needs a value after it\n", argv[i]);
      return std::nullopt;
    }
    const std::string_view value(argv[++i]);

    if (argument == "--master-ip") {
      options.master_ip = value;
    } else if (argument == "--master-port") {
      options.master_port = value;
    } else if (argument == "--owner-id") {
      const auto* end = value.data() + value.size();
      if (std::from_chars(value.data(), end, options.owner_id).ptr != end) {
        std::fprintf(stderr, "--owner-id got \"%.*s\", which is not a number\n",
                     static_cast<int>(value.size()), value.data());
        return std::nullopt;
      }
    } else {
      std::fprintf(stderr, "unknown argument %.*s\n", static_cast<int>(argument.size()),
                   argument.data());
      printUsage();
      return std::nullopt;
    }
  }
  return options;
}

void onGlfwError(int code, const char* description) {
  std::fprintf(stderr, "glfw error %d: %s\n", code, description);
}

}  // namespace

int main(int argc, char* argv[]) {
  srnp::srnp_print_setup("info");

  const auto options = parseArguments(argc, argv);
  if (!options) return 1;

  glfwSetErrorCallback(onGlfwError);
  if (!glfwInit()) {
    std::fprintf(stderr, "could not start GLFW. Is there a display?\n");
    return 1;
  }

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef GLFW_WAYLAND_APP_ID
  // Without this the window has no app id, and a Wayland compositor cannot
  // tell it apart from any other GLFW window.
  glfwWindowHintString(GLFW_WAYLAND_APP_ID, "pairview");
#endif

  GLFWwindow* window = glfwCreateWindow(1280, 840, "PairView", nullptr, nullptr);
  if (window == nullptr) {
    std::fprintf(stderr, "could not open a window\n");
    glfwTerminate();
    return 1;
  }

  glfwMakeContextCurrent(window);
  glfwSwapInterval(1);

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();

  // The one window is pinned to the viewport, so there is no layout worth
  // saving. Without this ImGui drops an imgui.ini wherever it was started.
  ImGui::GetIO().IniFilename = nullptr;

  // Follow the monitor's scale, so the text is the same physical size on a
  // HiDPI screen as on an ordinary one.
  float scale = 1.0f;
  glfwGetWindowContentScale(window, &scale, nullptr);
  if (scale <= 0.0f) scale = 1.0f;

  srnp::gui::applyStyle(scale);
  const auto fonts = srnp::gui::loadFonts(scale);

  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init("#version 150");

  {
    // The window is already up, so the ten-second connect attempt inside
    // SrnpLink happens behind a live UI rather than a frozen one.
    srnp::gui::SrnpLink link(options->master_ip, options->master_port, options->owner_id);
    srnp::gui::PairView view(link, fonts);

    while (!glfwWindowShouldClose(window)) {
      glfwWaitEventsTimeout(kFrameTimeout);

      ImGui_ImplOpenGL3_NewFrame();
      ImGui_ImplGlfw_NewFrame();
      ImGui::NewFrame();

      view.draw();

      ImGui::Render();
      int width = 0;
      int height = 0;
      glfwGetFramebufferSize(window, &width, &height);
      glViewport(0, 0, width, height);
      glClearColor(0.10f, 0.10f, 0.12f, 1.0f);
      glClear(GL_COLOR_BUFFER_BIT);
      ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

      glfwSwapBuffers(window);
    }
  }
  // The link is shut down before the GL context goes, so nothing srnp owns
  // is still running while the window is torn down.

  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();

  glfwDestroyWindow(window);
  glfwTerminate();
  return 0;
}
