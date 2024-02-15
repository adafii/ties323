#include "gui.h"
#include "client.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl2.h"
#include <format>
#include <GLFW/glfw3.h>
#include <iostream>
#include <ranges>
#include <string_view>

using std::literals::string_view_literals::operator""sv;
using std::literals::string_literals::operator""s;

namespace {

const auto columns = std::vector{"Date"s, "X-Sender"s, "To"s, "Subject"s};
constexpr auto glfw_error = "GLFW error: {}: {}\n"sv;
constexpr auto window_title = "POP3 Client"sv;

GLFWwindow* init_window() {
    glfwSetErrorCallback(
        [](auto error, auto description) { std::cerr << std::format(glfw_error, error, description); });

    if (!glfwInit()) {
        return nullptr;
    }

    auto window = glfwCreateWindow(1024, 800, window_title.data(), nullptr, nullptr);

    if (window == nullptr) {
        return nullptr;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(5);

    ImGui::CreateContext();
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL2_Init();

    return window;
}

}  // namespace

int start_gui(mail_storage& storage) {
    auto window = init_window();
    if (window == nullptr) {
        return EXIT_FAILURE;
    }

    auto background_color = ImVec4{0.4f, 0.4f, 0.4f, 1.00f};
    auto child_flags = ImGuiChildFlags_AlwaysUseWindowPadding;
    ImGui::GetStyle().FrameRounding = 4;
    ImGui::GetStyle().WindowBorderSize = 0;

    int32_t selected = -1;
    auto fetch_status_message = std::string{};
    char host_buffer[100] = "";
    char port_buffer[6] = "";
    char password_buffer[100] = "";
    char username_buffer[100] = "";
    bool use_tls = true;

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        ImGui_ImplOpenGL2_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        const auto viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->Pos);
        ImGui::SetNextWindowSize(viewport->Size);
        auto p_open = true;

        {
            ImGui::Begin("Main window", &p_open,
                         ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);

            {
                ImGui::BeginChild("Settings window", ImVec2(ImGui::GetContentRegionAvail().x * 1.f, 0.0f),
                                  child_flags | ImGuiChildFlags_AutoResizeY);

                ImGui::SetNextItemOpen(true, ImGuiCond_Once);
                if (ImGui::TreeNode("Settings")) {
                    ImGui::InputText("Host", host_buffer, IM_ARRAYSIZE(host_buffer), 0);
                    ImGui::InputText("Port", port_buffer, IM_ARRAYSIZE(port_buffer), ImGuiInputTextFlags_CharsDecimal);
                    ImGui::InputText("User", username_buffer, IM_ARRAYSIZE(username_buffer), 0);
                    ImGui::InputText("Password", password_buffer, IM_ARRAYSIZE(password_buffer),
                                     ImGuiInputTextFlags_Password);
                    ImGui::Checkbox("Use TLS", &use_tls);

                    ImGui::TreePop();
                }
                ImGui::EndChild();
            }

            {
                ImGui::BeginChild("Button window", ImVec2(ImGui::GetContentRegionAvail().x * 1.f, 0.0f),
                                  child_flags | ImGuiChildFlags_AutoResizeY);

                if (ImGui::Button("Get Mail")) {
                    auto host = std::string{host_buffer};
                    auto port = std::string{port_buffer};
                    auto username = std::string{username_buffer};
                    auto password = std::string{password_buffer};

                    if (host.empty() || port.empty() || username.empty() || password.empty()) {
                        fetch_status_message = "Empty fields!";
                    } else {
                        fetch_status_message.clear();
                        storage.clear();
                        selected = -1;

                        if (use_tls) {
                            fetch_status_message = pop3_get_mail_tls(username, password, host, port, storage);
                        } else {
                            fetch_status_message = pop3_get_mail(username, password, host, port, storage);
                        }
                    }
                }

                ImGui::SameLine(0.0, 20);
                ImGui::Text("%s", fetch_status_message.c_str());

                ImGui::EndChild();
            }

            {
                ImGui::BeginChild("Mails window", ImVec2(0.0f, ImGui::GetContentRegionAvail().y * 0.3f), child_flags);

                static ImGuiTableFlags table_flags = ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg |
                                                     ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable |
                                                     ImGuiTableFlags_Hideable | ImGuiTableFlags_BordersOuter |
                                                     ImGuiTableFlags_BordersV;

                ImGui::BeginTable("Mail table", static_cast<int>(columns.size()), table_flags);
                {
                    for (auto const& column : columns) {
                        ImGui::TableSetupColumn(column.c_str());
                    }

                    ImGui::TableHeadersRow();

                    for (std::size_t message_num = 0; message_num < storage.size(); message_num++) {
                        ImGui::TableNextRow();

                        const auto& headers = storage[message_num].headers;

                        ImGui::TableNextColumn();

                        auto cell = std::string{};
                        if (headers.contains(columns.front())) {
                            cell = headers.at(columns.front());
                        }

                        if (ImGui::Selectable(cell.c_str(), selected == static_cast<int32_t>(message_num),
                                              ImGuiSelectableFlags_SpanAllColumns)) {
                            selected = static_cast<int32_t>(message_num);
                        }

                        for (const auto& column : columns | std::views::drop(1)) {
                            ImGui::TableNextColumn();
                            cell.clear();

                            if (headers.contains(column)) {
                                cell = headers.at(column);
                            }

                            ImGui::Text("%s", cell.c_str());
                        }
                    }
                    ImGui::EndTable();
                }
                ImGui::EndChild();
            }

            {
                ImGui::BeginChild("Mail body", ImVec2(0.0f, ImGui::GetContentRegionAvail().y), child_flags);

                auto message = std::string{};
                if (selected >= 0 && selected < static_cast<int32_t>(storage.size())) {
                    message = storage[selected].body;
                }

                ImGui::TextWrapped("%s", message.c_str());

                ImGui::EndChild();
            }

            ImGui::End();
        }

        ImGui::Render();

        int display_w = 0;
        int display_h = 0;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(background_color.x * background_color.w, background_color.y * background_color.w,
                     background_color.z * background_color.w, background_color.w);
        glClear(GL_COLOR_BUFFER_BIT);

        ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
        glfwMakeContextCurrent(window);
        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL2_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return EXIT_SUCCESS;
}
