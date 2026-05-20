#include "Blupch.h"
#include "InputMap.h"
#include "Input.h"
#include "Blu/Core/Log.h"
#include "yaml-cpp/yaml.h"

namespace Blu
{
    InputMap& InputMap::Get()
    {
        static InputMap instance;
        return instance;
    }

    void InputMap::AddAction(const std::string& name, int keyCode)
    {
        ActionBinding b;
        b.Type = InputBindingType::Key;
        b.Code = keyCode;
        m_Actions[name].push_back(b);
    }

    void InputMap::AddMouseAction(const std::string& name, int button)
    {
        ActionBinding b;
        b.Type = InputBindingType::MouseButton;
        b.Code = button;
        m_Actions[name].push_back(b);
    }

    void InputMap::AddAxis(const std::string& name, int positiveKey, int negativeKey)
    {
        AxisBinding ax;
        ax.PositiveType = InputBindingType::Key;
        ax.PositiveCode = positiveKey;
        ax.NegativeType = InputBindingType::Key;
        ax.NegativeCode = negativeKey;
        m_Axes[name] = ax;
    }

    static bool IsBindingPressed(const ActionBinding& b)
    {
        if (b.Code < 0) return false;
        switch (b.Type)
        {
        case InputBindingType::Key:         return Input::IsKeyPressed(b.Code);
        case InputBindingType::MouseButton: return Input::IsMouseButtonPressed(b.Code);
        }
        return false;
    }

    bool InputMap::IsActionPressed(const std::string& name) const
    {
        auto it = m_Actions.find(name);
        if (it == m_Actions.end()) return false;
        for (const auto& b : it->second)
            if (IsBindingPressed(b)) return true;
        return false;
    }

    bool InputMap::IsActionJustPressed(const std::string& name) const
    {
        bool curr = IsActionPressed(name);
        bool prev = false;
        auto pit = m_PrevActionState.find(name);
        if (pit != m_PrevActionState.end()) prev = pit->second;
        m_PrevActionState[name] = curr;
        return curr && !prev;
    }

    float InputMap::GetAxis(const std::string& name) const
    {
        auto it = m_Axes.find(name);
        if (it == m_Axes.end()) return 0.0f;
        const AxisBinding& ax = it->second;

        float pos = 0.0f, neg = 0.0f;
        if (ax.PositiveCode >= 0)
        {
            if (ax.PositiveType == InputBindingType::Key)
                pos = Input::IsKeyPressed(ax.PositiveCode) ? 1.0f : 0.0f;
            else
                pos = Input::IsMouseButtonPressed(ax.PositiveCode) ? 1.0f : 0.0f;
        }
        if (ax.NegativeCode >= 0)
        {
            if (ax.NegativeType == InputBindingType::Key)
                neg = Input::IsKeyPressed(ax.NegativeCode) ? 1.0f : 0.0f;
            else
                neg = Input::IsMouseButtonPressed(ax.NegativeCode) ? 1.0f : 0.0f;
        }
        return pos - neg;
    }

    void InputMap::OnFrameEnd()
    {
        // Tick prev states for all known actions so JustPressed stays accurate
        for (auto& [name, _] : m_Actions)
            m_PrevActionState[name] = IsActionPressed(name);
    }

    void InputMap::Clear()
    {
        m_Actions.clear();
        m_Axes.clear();
        m_PrevActionState.clear();
    }

    void InputMap::SaveToFile(const std::filesystem::path& path) const
    {
        YAML::Emitter out;
        out << YAML::BeginMap;

        out << YAML::Key << "actions" << YAML::Value << YAML::BeginSeq;
        for (const auto& [name, bindings] : m_Actions)
        {
            for (const auto& b : bindings)
            {
                out << YAML::BeginMap;
                out << YAML::Key << "name" << YAML::Value << name;
                if (b.Type == InputBindingType::Key)
                    out << YAML::Key << "key" << YAML::Value << b.Code;
                else
                    out << YAML::Key << "mouse_button" << YAML::Value << b.Code;
                out << YAML::EndMap;
            }
        }
        out << YAML::EndSeq;

        out << YAML::Key << "axes" << YAML::Value << YAML::BeginSeq;
        for (const auto& [name, ax] : m_Axes)
        {
            out << YAML::BeginMap;
            out << YAML::Key << "name"         << YAML::Value << name;
            out << YAML::Key << "positive_key" << YAML::Value << ax.PositiveCode;
            out << YAML::Key << "negative_key" << YAML::Value << ax.NegativeCode;
            out << YAML::EndMap;
        }
        out << YAML::EndSeq;

        out << YAML::EndMap;

        std::ofstream f(path);
        if (f)
            f << out.c_str();
        else
            BLU_CORE_ERROR("InputMap: failed to write {0}", path.string());
    }

    void InputMap::LoadFromFile(const std::filesystem::path& path)
    {
        if (!std::filesystem::exists(path))
        {
            BLU_CORE_WARN("InputMap: file not found: {0}", path.string());
            return;
        }

        try
        {
            YAML::Node root = YAML::LoadFile(path.string());

            if (auto actions = root["actions"])
            {
                for (auto entry : actions)
                {
                    std::string name = entry["name"].as<std::string>();
                    if (entry["key"])
                        AddAction(name, entry["key"].as<int>());
                    else if (entry["mouse_button"])
                        AddMouseAction(name, entry["mouse_button"].as<int>());
                }
            }

            if (auto axes = root["axes"])
            {
                for (auto entry : axes)
                {
                    std::string name = entry["name"].as<std::string>();
                    int pos = entry["positive_key"] ? entry["positive_key"].as<int>() : -1;
                    int neg = entry["negative_key"] ? entry["negative_key"].as<int>() : -1;
                    AddAxis(name, pos, neg);
                }
            }

            BLU_CORE_INFO("InputMap: loaded {0}", path.string());
        }
        catch (const YAML::Exception& e)
        {
            BLU_CORE_ERROR("InputMap: YAML parse error in {0}: {1}", path.string(), e.what());
        }
    }
}
