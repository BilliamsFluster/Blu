#include "ContentBrowserPanel.h"
#include <imgui.h>
#include "Blu/Platform/OpenGL/OpenGLTexture.h"


namespace Blu
{
    constexpr std::string_view s_AssetsDirectory = "assets";

    ContentBrowserPanel::ContentBrowserPanel()
        :m_CurrentDirectory(s_AssetsDirectory)
    {
        m_FileIcons[".png"] = (Blu::Texture2D::Create("assets/textures/Wallpaper.png"));
        m_FolderOpenIcon = (Blu::Texture2D::Create("assets/textures/FolderOpen.png"));
        m_FolderClosedIcon = (Blu::Texture2D::Create("assets/textures/FolderClosed.png"));

    }
    std::string m_Filter;  // Add a filter string

    void ContentBrowserPanel::OnImGuiRender()
    {
        ImGui::Begin("Content Browser");

        // Split the window into 2 parts: left will be the directory tree view, right will be the directory content view
        ImGui::BeginChild("left pane", ImVec2(ImGui::GetWindowWidth() * 0.3f, 0), true);

        // Show directory tree view for the root directory and any expanded directories
        ShowDirectoryNodes(s_AssetsDirectory);

        ImGui::EndChild();

        ImGui::SameLine();

        // Show directory content view
        ImGui::BeginChild("right pane", ImVec2(0, 0), true);

        // If not at root directory, add button for going back to parent directory
        if (m_CurrentDirectory != s_AssetsDirectory)
        {
            if (ImGui::Button("<- Back"))
            {
                // Check if parent directory is still within assets directory
                std::filesystem::path potentialParentDir = m_CurrentDirectory.parent_path();
                if (potentialParentDir.string().find(s_AssetsDirectory.data()) != std::string::npos)
                {
                    m_CurrentDirectory = potentialParentDir;
                }
            }
        }

        // Add a filter input box
        static char filter[128] = "";
        ImGui::InputText("Filter", filter, IM_ARRAYSIZE(filter));

        // Add a filter type selection
        static const char* filterTypes[] = { "Name", "Extension" };
        static int currentFilterType = 0;
        ImGui::Combo("Filter Type", &currentFilterType, filterTypes, IM_ARRAYSIZE(filterTypes));

        // Show content of the current directory
        static float padding = 10.0f;
        static float thumbnailSize = 80.0f;
        float cellSize = thumbnailSize + padding;
        float panelWidth = ImGui::GetContentRegionAvail().x;

        int columnCount = (int)(panelWidth / cellSize);
        if (columnCount < 1) columnCount = 1;

        ImGui::Columns(columnCount, nullptr, false);

        for (auto& p : std::filesystem::directory_iterator(m_CurrentDirectory))
        {
            const auto& path = p.path();
            auto relativePath = std::filesystem::relative(path, m_CurrentDirectory);
            std::string filenameString = relativePath.filename().string();

            // Check if filename matches filter
            if (currentFilterType == 0 && std::string::npos == filenameString.find(filter))
                continue;

            if (currentFilterType == 1 && std::string::npos == relativePath.extension().string().find(filter))
                continue;

            ImGui::BeginGroup();  // Begin group for icon and filename

            if (p.is_directory())
            {
                auto closedID = reinterpret_cast<void*>(static_cast<intptr_t>(m_FolderClosedIcon->GetRendererID()));
                ImVec2 iconSize(50.0f, 50.0f);

                // Push transparent colors
                ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(0, 0, 0, 0));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(0, 0, 0, 0));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(0, 0, 0, 0));

                if (ImGui::ImageButton(closedID, iconSize, ImVec2(0, 1), ImVec2(1, 0))) // Make the image a button
                {
                    m_CurrentDirectory = path;
                }

                // Pop colors to reset them
                ImGui::PopStyleColor(3);
            }
            else
            {
                // Render file icon and make it selectable as necessary
            }

            // Display filename
            ImGui::TextWrapped("%s", filenameString.c_str());

            ImGui::EndGroup();  // End group for icon and filename

            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("%s", filenameString.c_str());
            }

            ImGui::NextColumn();
        }

        ImGui::Columns(1);

        ImGui::EndChild();

        ImGui::End();
    }
    
    void ContentBrowserPanel::SortEntries(std::vector<std::filesystem::directory_entry>& entries, int sort_option)
    {
        // Implement sorting based on sort_option
    }
    void ContentBrowserPanel::ShowDirectoryNodes(const std::filesystem::path& directoryPath)
    {

        std::string directoryName = directoryPath.filename().string();

        // Check if this directory is expanded
        auto dir_it = m_DirectoryExpandedState.find(directoryPath.string());
        bool expanded = dir_it != m_DirectoryExpandedState.end() ? dir_it->second : false;

        auto& io = ImGui::GetIO();
        auto closedID = reinterpret_cast<void*>(static_cast<intptr_t>(m_FolderClosedIcon->GetRendererID()));  // Using the folder closed icon texture ID
        auto openID = reinterpret_cast<void*>(static_cast<intptr_t>(m_FolderOpenIcon->GetRendererID()));  // Using the folder open icon texture ID
        ImVec2 iconSize(30.0f, 30.0f);

        // Select proper icon and set tree node flags
        void* iconID = expanded ? openID : closedID;
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;
        flags |= expanded ? ImGuiTreeNodeFlags_DefaultOpen : 0;

        ImGui::Image(iconID, iconSize, ImVec2(0, 1), ImVec2(1, 0));  // Display the open or closed folder icon
        ImGui::SameLine();

        // Start a tree node for this directory
        bool nodeOpen = ImGui::TreeNodeEx(directoryName.c_str(), flags);

        // If the node is selected (clicked), change the current directory
        if (ImGui::IsItemClicked())
        {
            m_CurrentDirectory = directoryPath;
            m_DirectoryExpandedState[directoryPath.string()] = !expanded;
        }

        if (nodeOpen)
        {
            // Iterate over each subdirectory of this directory
            for (auto& p : std::filesystem::directory_iterator(directoryPath))
            {
                if (p.is_directory())
                {
                    ShowDirectoryNodes(p.path());
                }
            }

            ImGui::TreePop();
        }
    }
    
}