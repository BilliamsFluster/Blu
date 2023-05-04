workspace "Blu"
	architecture "x64"
	startproject "Azure"
	
	configurations
	{
		"Debug",
		"Release",
		"Dist"
	}

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.archetecture}"

project "Blu"
	location "Blu"
	kind "SharedLib"
	language "C++"

	targetdir ("bin/" .. outputdir .. "/%{prj.name}")
	objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

	pchheader "Blupch.h"
	pchsource "Blu/engine/src/Blupch.cpp"

	files
	{
		"%{prj.name}/engine/src/**.h",
		"%{prj.name}/engine/src/**.cpp",

	}

	includedirs 
	{
		"$(SolutionDir)/Blu/engine/ExternalDependencies/spdlog/include",
		"$(SolutionDir)Blu/engine/src",
		"."
	}
	
	buildoptions { "/wd4251" }


	filter "system:windows"
		cppdialect "C++20"
		staticruntime "On"
		systemversion "latest"

		defines
		{
			"BLU_PLATFORM_WINDOWS",
			"BLU_BUILD_DLL"
		}

		postbuildcommands
		{
			("{COPY} %{cfg.buildtarget.relpath} ../bin/" ..outputdir.. "/Azure")
		}

	filter "configurations:Debug"
		defines "BLU_RELEASE"
		symbols "On"

	filter "configurations:Release"
		defines "BLU_DEBUG"
		optimize "On"

	filter "configurations:Dist"
		defines "BLU_DIST"
		optimize "On"
	
		--multithreaded 
		-- refer to premake wiki on more info
	-- filter {"system:windows", "configurations:Release"}
	-- buildoptions "/MT"

project "Azure"
	location "Azure"
	kind "ConsoleApp"
	language "C++"

	targetdir ("bin/" .. outputdir .. "/%{prj.name}")
	objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

	files
	{
		"%{prj.name}/engine/src/**.h",
		"%{prj.name}/engine/src/**.cpp",
		"%{prj.name}/src/**.cpp"

	}

	includedirs 
	{
		"$(SolutionDir)Blu/engine/ExternalDependencies/spdlog/include",
		"$(SolutionDir)Blu/engine/src"
		--"Blu/engine/src",
		
	}
	
	links
	{
		"Blu"
	}
	buildoptions { "/wd4251" }

	filter "system:windows"
		cppdialect "C++20"
		staticruntime "On"
		systemversion "latest"

		defines
		{
			"BLU_PLATFORM_WINDOWS"
		}

	filter "configurations:Debug"
		defines "BLU_RELEASE"
		symbols "On"

	filter "configurations:Release"
		defines "BLU_DEBUG"
		optimize "On"

	filter "configurations:Dist"
		defines "BLU_DIST"
		optimize "On"

