#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <filesystem>

namespace Blu
{
    // Binding types
    enum class InputBindingType { Key, MouseButton };

    struct ActionBinding
    {
        InputBindingType Type    = InputBindingType::Key;
        int              Code    = -1;   // GLFW key or mouse button
    };

    // A named axis maps two keys to a [-1, 0, +1] value.
    struct AxisBinding
    {
        InputBindingType PositiveType    = InputBindingType::Key;
        int              PositiveCode    = -1;
        InputBindingType NegativeType    = InputBindingType::Key;
        int              NegativeCode    = -1;
    };

    // Singleton Input Action Map — query by action/axis name instead of raw key codes.
    // Load from YAML or build programmatically at startup.
    //
    // YAML format:
    //   actions:
    //     - name: Jump
    //       key: 32          # GLFW_KEY_SPACE
    //     - name: Fire
    //       mouse_button: 0
    //   axes:
    //     - name: Horizontal
    //       positive_key: 68   # D
    //       negative_key: 65   # A
    //     - name: Vertical
    //       positive_key: 87   # W
    //       negative_key: 83   # S
    class InputMap
    {
    public:
        static InputMap& Get();

        // Add an action bound to a keyboard key (GLFW key code)
        void AddAction(const std::string& name, int keyCode);
        // Add an action bound to a mouse button
        void AddMouseAction(const std::string& name, int button);

        // Add an axis pair (+key / -key)
        void AddAxis(const std::string& name, int positiveKey, int negativeKey);

        // Returns true while the action binding is held
        bool IsActionPressed(const std::string& name) const;
        // Returns true on the frame the binding goes from released to pressed
        bool IsActionJustPressed(const std::string& name) const;

        // Returns a value in {-1, 0, +1} based on which axis keys are held
        float GetAxis(const std::string& name) const;

        // Serialise current map to YAML file
        void SaveToFile(const std::filesystem::path& path) const;
        // Load (or reload) from YAML file — merges with any existing bindings
        void LoadFromFile(const std::filesystem::path& path);

        // Clear all bindings
        void Clear();

        // Must be called once per frame to tick JustPressed state
        void OnFrameEnd();

    private:
        InputMap() = default;

        std::unordered_map<std::string, std::vector<ActionBinding>> m_Actions;
        std::unordered_map<std::string, AxisBinding>                m_Axes;

        // Track previous frame state for JustPressed detection
        mutable std::unordered_map<std::string, bool> m_PrevActionState;
    };
}
