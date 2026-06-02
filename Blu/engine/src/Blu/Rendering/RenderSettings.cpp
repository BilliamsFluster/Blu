#include "Blupch.h"
#include "RenderSettings.h"

namespace Blu
{
    RenderPath RenderSettings::s_Path           = RenderPath::Forward;
    bool       RenderSettings::s_UseGBufferSSAO  = false;
}
