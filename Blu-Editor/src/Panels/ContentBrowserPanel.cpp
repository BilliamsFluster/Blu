#include "ContentBrowserPanel.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <fstream>
#include "Blu/Core/MouseCodes.h"
#include "Blu/Core/Input.h"
#include "Blu/Platform/OpenGL/OpenGLTexture.h"



namespace Blu
{
    constexpr std::string_view s_AssetsDirectory = "assets";

    ContentBrowserPanel::ContentBrowserPanel()
        :m_CurrentDirectory(s_AssetsDirectory)
    {
        m_FileIcons[".png"] = (Blu::Texture2D::Create("assets/textures/FileIcon.png"));
        m_FolderOpenIcon = (Blu::Texture2D::Create("assets/textures/FolderOpen.png"));
        m_FolderClosedIcon = (Blu::Texture2D::Create("assets/textures/FolderClosed.png"));

    }
    std::string m_Filter;  // Add a filter string
    std::filesystem::path rightClickedItemPath;
   
    void RecursiveSearch(const std::filesystem::path& directory, const std::string& filter)
    {
        for (auto& p : std::filesystem::recursive_directory_iterator(directory))
        {
            const auto& path = p.path();
            auto relativePath = std::filesystem::relative(path, directory);
            std::string filenameString = relativePath.filename().string();
            std::string extensionString = relativePath.extension().string();

            // Apply filter
            if (filenameString.find(filter) != std::string::npos ||
                extensionString.find(filter) != std::string::npos)
            {
                // Display the file or folder, similar to how you do in the directory_iterator loop
            }
        }
    }
    void ContentBrowserPanel::OnImGuiRender()
    {
        static std::filesystem::path s_RenamingPath;

        ImGui::Begin("Content Browser");

        // Split the window into 2 parts: left will be the directory tree view, right will be the directory content view
        ImGui::BeginChild("left pane", ImVec2(ImGui::GetWindowWidth() * 0.3f, 0), true);

        if (ImGui::Button("Add New"))
        {
            ImGui::OpenPopup("AddNew");
        }
        

        if (ImGui::BeginPopup("AddNew"))    
        {
            if (ImGui::MenuItem("New Folder"))
            {
                std::filesystem::path newFolderPath = m_CurrentDirectory / "EmptyFolder";
                int i = 0;
                while (std::filesystem::exists(newFolderPath)) {
                    newFolderPath = m_CurrentDirectory / ("EmptyFolder" + std::to_string(i++));
                }
                std::filesystem::create_directory(newFolderPath);
                ImGui::CloseCurrentPopup();
            }
            if (ImGui::MenuItem("New File"))  // Add a delete option to the context menu
            {
                std::filesystem::path newFilePath = m_CurrentDirectory / "EmptyFile.txt";
                int i = 0;
                while (std::filesystem::exists(newFilePath)) {
                    newFilePath = m_CurrentDirectory / ("EmptyFile" + std::to_string(i++) + ".txt");
                }
                std::ofstream newFile(newFilePath);
                ImGui::CloseCurrentPopup();

            }
            ImGui::EndPopup();
            
            
        }
        // Show directory tree view for the root directory and any expanded directories
        ShowDirectoryNodes(s_AssetsDirectory);

        ImGui::EndChild();

        ImGui::SameLine();

        // Show directory content view

       
        ImGui::BeginChild("right pane", ImVec2(0, 0), true);
        for (auto it = m_NavigationHistory.begin(); it != m_NavigationHistory.end(); ++it)
        {
            // If this isn't the first directory in the history, add a separator
            if (it != m_NavigationHistory.begin())
            {
                ImGui::SameLine(); ImGui::Text(" > "); ImGui::SameLine();
            }

            if (ImGui::Button(it->filename().string().c_str()))
            {
                // Navigate directly to this directory
                m_CurrentDirectory = *it;
                // Remove all directories after the clicked one
                m_NavigationHistory.erase(std::next(it), m_NavigationHistory.end());
                break;
            }
        }

        // Top operation panel
        ImGui::BeginChild("Operation Panel", ImVec2(0, 40), true);

       
       

        // Filter Type Selection Combo box
        static const char* filterTypes[] = { "Name", "Date Modified", "Size", "Extension" };
        static int currentFilterType = 0;
        ImVec2 contentRegionAvailable = ImGui::GetContentRegionAvail();
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{ 4, 4 });
        ImGui::PopStyleVar();
        //ImGui::SameLine(contentRegionAvailable.x - lineHeight * 0.5f - 50.0f);
        ImGui::PushItemWidth(100.0f);  // set width of the combo box
        ImGui::Combo("##Filter Type", &currentFilterType, filterTypes, IM_ARRAYSIZE(filterTypes));
        ImGui::PopItemWidth();  // reset width
        ImGui::SameLine();
        
        // Filter Input Text box
        static char filter[128] = "";
        ImGui::PushItemWidth(contentRegionAvailable.x - 100.0f);  // set width of the combo box
        ImGui::InputTextWithHint("##Filter", "Search Content", filter, IM_ARRAYSIZE(filter));
        ImGui::PopItemWidth();  // reset width

        // Add logic for filtering and sorting here

        ImGui::EndChild();


        if (m_CurrentDirectory != s_AssetsDirectory)
        {
            if (ImGui::Button("<- Back"))
            {
                // Check if parent directory is still within assets directory
                std::filesystem::path potentialParentDir = m_CurrentDirectory.parent_path();
                if (potentialParentDir.string().find(s_AssetsDirectory.data()) != std::string::npos)
                {
                    // Change the current directory
                    m_CurrentDirectory = potentialParentDir;
                    // Replace the navigation history with a path to the current directory
                    m_NavigationHistory = GetDirectoryPath(m_CurrentDirectory);
                }
            }
        }

       

      

        // Show content of the current directory
        static float padding = 10.0f;
        static float thumbnailSize = 80.0f;
        float cellSize = thumbnailSize + padding;
        float panelWidth = ImGui::GetContentRegionAvail().x;

        int columnCount = (int)(panelWidth / cellSize);
        if (columnCount < 1) columnCount = 1;

        m_ObjectClicked = false;
        ImGui::Columns(columnCount, nullptr, false);
        static std::string s_SelectedFilename;
        //
        for (auto& p : std::filesystem::directory_iterator(m_CurrentDirectory)) 
        {
            const auto& path = p.path();
            auto relativePath = std::filesystem::relative(path, m_CurrentDirectory);
            std::string filenameString = relativePath.filename().string();
            std::string extensionString = relativePath.extension().string();

            // Filtering based on filter type
            bool skip = false; // flag to skip the current file/directory
            switch (currentFilterType) {
            case 0: // Name filter
                if (filenameString.find(filter) == std::string::npos)
                    skip = true;
                break;

            case 1: // Date Modified filter
                //  logic for date modified filter
                break;

            case 2: // Size filter
                //  logic for size filter
                break;

            case 3: // Extension filter
                if (extensionString != filter)
                    skip = true;
                break;

            default:
                break;
            }

            if (skip) continue;

            ImGui::BeginGroup();  // Begin group for icon and filename
    
            if (p.is_directory())
            {


                auto closedID = reinterpret_cast<void*>(static_cast<intptr_t>(m_FolderClosedIcon->GetRendererID()));
                ImVec2 iconSize(50.0f, 50.0f);

                ImGui::PushID(path.string().c_str());
                ImGui::Image(closedID, iconSize, ImVec2(0, 1), ImVec2(1, 0));

                ImGui::SetCursorPosY(ImGui::GetCursorPosY() - iconSize.y);
                if (ImGui::InvisibleButton(filenameString.c_str(), iconSize))
                {
                    s_SelectedFilename = filenameString;

                }
                if (s_SelectedFilename == filenameString) {
                    ImDrawList* draw_list = ImGui::GetWindowDrawList();
                    ImVec2 min = ImGui::GetItemRectMin();
                    ImVec2 max = ImGui::GetItemRectMax();

                    draw_list->AddRect(min, max, IM_COL32(30, 151, 201, 255), 1, 0, 3);  // blue outline
                }

                if (ImGui::IsItemHovered())
                {

                    if (ImGui::IsMouseDoubleClicked(0))
                    {
                        // Double click: Change the current directory
                        m_CurrentDirectory = path;

                        m_NavigationHistory = GetDirectoryPath(m_CurrentDirectory);

                    }
                    if (ImGui::IsMouseClicked(1))
                    {
                        rightClickedItemPath = path;
                        m_ObjectClicked = true;
                    }
                }
                else if (ImGui::IsMouseClicked(0) && !ImGui::IsAnyItemHovered())
                {
                    s_SelectedFilename = "";
                }





                ImGui::PopID();
            }
            else
            {
                // Render file icon and make it selectable as necessary
                auto fileID = reinterpret_cast<void*>(static_cast<intptr_t>(m_FileIcons[".png"]->GetRendererID()));
                ImVec2 iconSize(50.0f, 50.0f);

                ImGui::PushID(path.string().c_str());
                ImGui::Image(fileID, iconSize, ImVec2(0, 1), ImVec2(1, 0));

                ImGui::SetCursorPosY(ImGui::GetCursorPosY() - iconSize.y);
                if (ImGui::InvisibleButton(filenameString.c_str(), iconSize))
                {
                    s_SelectedFilename = filenameString;
                }
                if (s_SelectedFilename == filenameString) {
                    ImDrawList* draw_list = ImGui::GetWindowDrawList();
                    ImVec2 min = ImGui::GetItemRectMin();
                    ImVec2 max = ImGui::GetItemRectMax();


                    draw_list->AddRect(min, max, IM_COL32(30, 151, 201, 255), 1, 0, 3);  // blue outline
                }
                if (ImGui::IsItemHovered())
                {
                    m_ObjectClicked = true;


                    if (ImGui::IsMouseClicked(1))
                    {
                        rightClickedItemPath = path;
                        m_ObjectClicked = true;

                    }
                }
                else if (ImGui::IsMouseClicked(0) && !ImGui::IsAnyItemHovered())
                {
                    s_SelectedFilename = "";
                }
                ImGui::PopID();
            }


            if (ImGui::IsMouseClicked(1) && !m_ObjectClicked)
            {
                rightClickedItemPath = "";  // clicked on empty space
            }

            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {

                std::string payloadPath = path.string();

                // Set payload to carry the path of the file being dragged
                ImGui::SetDragDropPayload("CONTENT_BROWSER_ITEM", payloadPath.c_str(), payloadPath.size() + 1); // +1 to include the null terminator

                // Get current mouse position
                ImVec2 mousePos = ImGui::GetMousePos();

                // Size of your image
                ImVec2 imageSize = ImVec2(50, 50);

                // Calculate the top left position for the image
                // to make the image centered around the mouse position
                ImVec2 imagePos = ImVec2(mousePos.x - imageSize.x * 0.5f, mousePos.y - imageSize.y * 0.5f);
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

                // Create an overlay at mouse position
                ImGui::SetNextWindowPos(imagePos);
                ImGui::SetNextWindowSize(imageSize);

                // Make the window background transparent and disable title bar, resize, move, and scrollbars
                ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                    ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                    ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoInputs |
                    ImGuiWindowFlags_NoBackground;

                // Create the overlay window
                ImGui::Begin("ImageOverlay", nullptr, windowFlags);

                if (std::filesystem::is_directory(payloadPath))
                {
                    ImGui::Image((void*)(intptr_t)m_FolderOpenIcon->GetRendererID(), imageSize, ImVec2(0, 1), ImVec2(1, 0));
                }
                else
                {
                    ImGui::Image((void*)(intptr_t)m_FileIcons[".png"]->GetRendererID(), imageSize, ImVec2(0, 1), ImVec2(1, 0));
                }
                ImGui::End();
                ImGui::PopStyleVar();

                // Display a preview (could be anything, e.g. the filename, an icon...)
                ImGui::Text("%s", filenameString.c_str());

                ImGui::EndDragDropSource();
            }

            if (ImGui::BeginDragDropTarget())
            {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
                {
                    std::filesystem::path payloadPath = std::string(reinterpret_cast<const char*>(payload->Data));

                    // Check if the directory is not dragged onto itself
                    if (path != payloadPath) {
                        std::filesystem::path destinationPath = path / payloadPath.filename();

                        // Move file or directory
                        try {
                            std::filesystem::rename(payloadPath, destinationPath);
                            // Refresh your directory viewer here
                        }
                        catch (std::filesystem::filesystem_error& e)
                        {
                            BLU_CORE_ASSERT(false, e.what());
                        }
                    }




                }

                ImGui::EndDragDropTarget();
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
        //
        //for (auto& p : std::filesystem::directory_iterator(m_CurrentDirectory))
        //{
        //    const auto& path = p.path();
        //    auto relativePath = std::filesystem::relative(path, m_CurrentDirectory);
        //    std::string filenameString = relativePath.filename().string();


        //    // Check if filename matches filter
        //    if (currentFilterType == 0 && std::string::npos == filenameString.find(filter))
        //        continue;

        //    if (currentFilterType == 1 && std::string::npos == relativePath.extension().string().find(filter))
        //        continue;

        //    ImGui::BeginGroup();  // Begin group for icon and filename
        //    
        //   if directory goes here
        //}

        if (!rightClickedItemPath.empty() && ImGui::BeginPopupContextWindow())
        {
            if (ImGui::MenuItem("Rename"))
            {
                s_RenamingPath = rightClickedItemPath;
                ImGui::OpenPopup("Rename Dialog");
            }
            if (ImGui::MenuItem("Delete"))  // Add a delete option to the context menu
            {
                if (std::filesystem::exists(rightClickedItemPath))
                {
                    std::filesystem::remove_all(rightClickedItemPath);  // Delete the file or folder
                    rightClickedItemPath.clear();  // Clear the path as it is no longer valid
                }
            }
            ImGui::EndPopup();
        }
        if (rightClickedItemPath.empty() && ImGui::BeginPopupContextWindow())
        {
            if (ImGui::MenuItem("New Folder"))
            {
                std::filesystem::path newFolderPath = m_CurrentDirectory / "EmptyFolder";
                int i = 0;
                while (std::filesystem::exists(newFolderPath)) {
                    newFolderPath = m_CurrentDirectory / ("EmptyFolder" + std::to_string(i++));
                }
                std::filesystem::create_directory(newFolderPath);
            }
            if (ImGui::MenuItem("New File"))  // Add a delete option to the context menu
            {
                std::filesystem::path newFilePath = m_CurrentDirectory / "EmptyFile.txt"; 
                int i = 0;
                while (std::filesystem::exists(newFilePath)) {
                    newFilePath = m_CurrentDirectory / ("EmptyFile" + std::to_string(i++) + ".txt");
                }
                std::ofstream newFile(newFilePath);
            }
            ImGui::EndPopup();

        }
        if (!s_RenamingPath.empty()) {
            ImGui::OpenPopup("Rename Dialog");
        }
        static char renameBuffer[128] = "";

        if (ImGui::BeginPopupModal("Rename Dialog", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {

            // Only copy the filename to the buffer when the popup is first opened
            if (ImGui::IsWindowAppearing()) {
                std::string filename = s_RenamingPath.filename().string();
                std::copy(filename.begin(), filename.end(), renameBuffer);
                renameBuffer[filename.size()] = '\0';  // null terminate
            }

            ImGui::InputText("New name", renameBuffer, IM_ARRAYSIZE(renameBuffer));

            if (ImGui::Button("Rename")) {
                std::filesystem::path newPath = s_RenamingPath.parent_path() / renameBuffer;
                if (!std::filesystem::exists(newPath)) {
                    std::filesystem::rename(s_RenamingPath, newPath);
                    s_RenamingPath.clear();
                    ImGui::CloseCurrentPopup();
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) {
                s_RenamingPath.clear();
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        ImGui::Columns(1);

        ImGui::EndChild();

        ImGui::End();
    }
    
    void ContentBrowserPanel::SortEntries(std::vector<std::filesystem::directory_entry>& entries, int sort_option)
    {
        // Currently only sorting by name is implemented
        switch (sort_option) {
        case 0: // Name
            std::sort(entries.begin(), entries.end(),
                [](const std::filesystem::directory_entry& a, const std::filesystem::directory_entry& b) {
                    return a.path().filename().string() < b.path().filename().string();
                });
            break;
            // TODO: Implement sorting by date, size and type
        }
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
            // Toggle the expanded state
            m_DirectoryExpandedState[directoryPath.string()] = !expanded;
            // If the directory is opened...
            if (m_DirectoryExpandedState[directoryPath.string()])
            {
                // Change the current directory
                m_CurrentDirectory = directoryPath;
                // Replace the navigation history with a path to the current directory
                m_NavigationHistory = GetDirectoryPath(m_CurrentDirectory);
            }
            // Do nothing if the directory is closed
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

    std::deque<std::filesystem::path> ContentBrowserPanel::GetDirectoryPath(const std::filesystem::path& directory)
    {
        std::deque<std::filesystem::path> path;
        for (std::filesystem::path p = directory; p != s_AssetsDirectory; p = p.parent_path())
        {
            path.push_front(p);
        }
        path.push_front(s_AssetsDirectory);
        return path;
    }

    void ContentBrowserPanel::CreateNewFile(const std::filesystem::path& directory, const std::string& baseName)
    {
        std::filesystem::path newFilePath = directory / baseName;
        int i = 0;
        while (std::filesystem::exists(newFilePath))
        {
            newFilePath = directory / (baseName + std::to_string(i++) + ".txt");
        }
        std::ofstream newFile(newFilePath);
    }

    void ContentBrowserPanel::CreateNewFolder(const std::filesystem::path& directory, const std::string& baseName)
    {
        std::filesystem::path newFolderPath = directory / baseName;
        int i = 0;
        while (std::filesystem::exists(newFolderPath))
        {
            newFolderPath = directory / (baseName + std::to_string(i++));
        }
        std::filesystem::create_directory(newFolderPath);
    }

}