workspace "Blu"
	architecture "x64"
	startproject "Azure"
	
	configurations
	{
		"Debug",
		"Release",
		"Dist"
	}
	outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"
	--Include directories relative to root folder(solutuin dir)
	IncludeDir = {}
	
	IncludeDir["GLFW"] = "Blu/engine/ExternalDependencies/GLFW/include"
	IncludeDir["Glad"] = "Blu/engine/ExternalDependencies/Glad/include"
	IncludeDir["ImGui"] = "Blu/engine/ExternalDependencies/imgui"
	IncludeDir["glm"] = "Blu/engine/ExternalDependencies/glm"
	IncludeDir["stb_image"] = "Blu/engine/ExternalDependencies/stb_image"




	
	include "Blu/engine/ExternalDependencies/GLFW"
	include "Blu/engine/ExternalDependencies/Glad"
	include "Blu/engine/ExternalDependencies/imgui"




	


project "Blu"
	location "Blu"
	kind "StaticLib"
	language "C++"
	cppdialect "C++20"
	staticruntime "on"

	targetdir ("bin/" .. outputdir .. "/%{prj.name}")
	objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

	pchheader "Blupch.h"
	pchsource "Blu/engine/src/Blupch.cpp"

	files
	{
		"%{prj.name}/engine/src/**.h",
		"%{prj.name}/engine/src/**.cpp",
		"%{prj.name}/engine/ExternalDependencies/stb_image/**.cpp",
		"%{prj.name}/engine/ExternalDependencies/glm/**.h",
		"%{prj.name}/engine/ExternalDependencies/glm/**.hpp",
		"%{prj.name}/engine/ExternalDependencies/glm/**.inl",
		"%{prj.name}/engine/ExternalDependencies/stb_image/**.h"



	}   

	includedirs 
	{
		"$(SolutionDir)/Blu/engine/ExternalDependencies/spdlog/include",
		"$(SolutionDir)Blu/engine/src",
		"%{IncludeDir.GLFW}",
		"%{IncludeDir.Glad}",
		"%{IncludeDir.ImGui}",
		"%{IncludeDir.glm}",
		"%{IncludeDir.stb_image}"





	}
	
	links
	{
		"GLFW",
		"Glad",
		"opengl32",
		"ImGui",
		"dwmapi"
		
	}
	
	--buildoptions { "/wd4251" } for dll


	filter "system:windows"
		systemversion "latest"

	--links {"gdi32", "dwmapi", "User32" }
		--"opengl32.lib"
		defines
		{
			"BLU_PLATFORM_WINDOWS",
			"BLU_BUILD_DLL",
			"GLFW_INCLUDE_NONE"
		}

		

	filter "configurations:Debug"
		defines "BLU_DEBUG"
		buildoptions "/MTd"
		symbols "on"
		linkoptions { "/NODEFAULTLIB:MSVCRT" }

	filter "configurations:Release"
		defines "BLU_RELEASE"
		buildoptions "/MT"
		optimize "on"

	filter "configurations:Dist"
		defines "BLU_DIST"
		buildoptions "/MT"
		optimize "on"
	
		--multithreaded 
		-- refer to premake wiki on more info
	-- filter {"system:windows", "configurations:Release"}
	-- buildoptions "/MT"

project "Azure"
	location "Azure"
	kind "ConsoleApp"
	language "C++"
	cppdialect "C++20"
	staticruntime "on" 


	targetdir ("bin/" .. outputdir .. "/%{prj.name}")
	objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

	files
	{
		"%{prj.name}/engine/src/**.h",
		"%{prj.name}/engine/src/**.cpp", 
		"%{prj.name}/src/**.cpp",
		

	}

	includedirs 
	{
		"$(SolutionDir)Blu/engine/ExternalDependencies/spdlog/include",
		"$(SolutionDir)Blu/engine/ExternalDependencies/glm",
		"$(SolutionDir)Blu/engine/src"

		
		
	}
	
	links
	{
		"Blu"

	}
	--buildoptions { "/wd4251" } for dll

	filter "system:windows"
		systemversion "latest"

		defines
		{
			"BLU_PLATFORM_WINDOWS"
		}

	filter "configurations:Debug"
		defines "BLU_DEBUG"
		symbols "on"

	filter "configurations:Release"
		defines "BLU_RELEASE"
		optimize "on"

	filter "configurations:Dist"
		defines "BLU_DIST"
		optimize "on"

