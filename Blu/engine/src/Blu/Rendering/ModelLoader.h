#pragma once
#include <string>
#include "Blu/Core/Core.h"
#include "Mesh.h"

namespace Blu
{
    class ModelLoader
    {
    public:
        static Shared<Model> Load(const std::string& path);
    };
}
