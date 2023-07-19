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
	
	IncludeDir["Glad"] = "Blu/engine/ExternalDependencies/Glad/include"
	IncludeDir["GLFW"] = "Blu/engine/ExternalDependencies/GLFW/include"
	IncludeDir["ImGui"] = "Blu/engine/ExternalDependencies/imgui"
	IncludeDir["glm"] = "Blu/engine/ExternalDependencies/glm"
	IncludeDir["stb_image"] = "Blu/engine/ExternalDependencies/stb_image"
	IncludeDir["entt"] = "Blu/engine/ExternalDependencies/entt/include"
	IncludeDir["yaml"] = "Blu/engine/ExternalDependencies/yaml/include"
	IncludeDir["ImGuizmo"] = "Blu/engine/ExternalDependencies/ImGuizmo"
	IncludeDir["box2d"] = "Blu/engine/ExternalDependencies/box2d/include"
	IncludeDir["mono"] = "Blu/engine/ExternalDependencies/mono/include"


	LibraryDir = {}

	LibraryDir["mono"] = "$(SolutionDir)Blu/engine/ExternalDependencies/mono/lib/%{cfg.buildcfg}"

	Library = {}

	Library["mono"] = "%{LibraryDir.mono}/libmono-static-sgen.lib"

	
	include "Blu/engine/ExternalDependencies/GLFW"
	include "Blu/engine/ExternalDependencies/Glad"
	include "Blu/engine/ExternalDependencies/imgui"
	include "Blu/engine/ExternalDependencies/yaml"
	include "Blu/engine/ExternalDependencies/box2d"
	




	


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
		"%{prj.name}/engine/ExternalDependencies/stb_image/**.h",
		"%{prj.name}/engine/ExternalDependencies/ImGuizmo/ImGuizmo.h",
		"%{prj.name}/engine/ExternalDependencies/ImGuizmo/ImGuizmo.cpp"



	}   

	includedirs 
	{
		"$(SolutionDir)/Blu/engine/ExternalDependencies/spdlog/include",
		"$(SolutionDir)Blu/engine/src",
		"%{IncludeDir.GLFW}",
		"%{IncludeDir.Glad}",
		"%{IncludeDir.ImGui}",
		"%{IncludeDir.glm}",
		"%{IncludeDir.stb_image}",
		"%{IncludeDir.entt}",
		"%{IncludeDir.mono}",
		"%{IncludeDir.yaml}",
		"%{IncludeDir.ImGuizmo}",
		"%{IncludeDir.box2d}",





	}
	
	links
	{
		"GLFW",
		"Glad",
		"opengl32",
		"ImGui",
		"yaml",
		"dwmapi",
		"box2d",
		"%{Library.mono}"
		
	}
	filter {"files:Blu/engine/ExternalDependencies/ImGuizmo/*.cpp"}
	flags {"NoPCH"}

	
	--buildoptions { "/wd4251" } for dll


	filter "system:windows"
		systemversion "latest"

	--links {"gdi32", "dwmapi", "User32" }
		--"opengl32.lib"
		defines
		{
			"BLU_PLATFORM_WINDOWS",
			"BLU_BUILD_DLL",
			"GLFW_INCLUDE_NONE",
			"NOMINMAX",
			"IMGUI_DEFINE_MATH_OPERATORS"
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
		"$(SolutionDir)Blu/engine/src",
		"%{IncludeDir.entt}",
		


		
		
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

project "Blu-Editor"
	location "Blu-Editor"
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
		"%{prj.name}/engine/ExternalDependencies/imgui/**.cpp",
		"%{prj.name}/engine/ExternalDependencies/imgui/**.h",
		"%{prj.name}/engine/ExternalDependencies/GLFW/**.cpp",
		"%{prj.name}/engine/ExternalDependencies/Glad/**.h"

	}

	includedirs 
	{
		"$(SolutionDir)Blu/engine/ExternalDependencies/spdlog/include",
		"$(SolutionDir)Blu/engine/ExternalDependencies/glm",
		"$(SolutionDir)Blu/engine/src",
		"$(SolutionDir)Blu/engine/ExternalDependencies/imgui",
		"$(SolutionDir)Blu/engine/ExternalDependencies/GLFW/include",
		"$(SolutionDir)Blu/engine/ExternalDependencies/Glad/include",
		"%{IncludeDir.entt}",
		"%{IncludeDir.yaml}",
		"%{IncludeDir.ImGuizmo}"





		
		
	}
	
	links
	{
		"Blu",
		"ImGui",
		"GLFW",
		"yaml",
		"Glad"


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
