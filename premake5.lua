workspace "Blu"
	architecture "x64"
	startproject "Blu-Editor"
	
	configurations
	{
		"Debug",
		"Release",
		"Dist"
	}
	outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"
	--Include directories relative to root folder(solution dir)
	IncludeDir = {}
	
	IncludeDir["Glad"] =		"$(SolutionDir)/Blu/engine/ExternalDependencies/Glad/include"
	IncludeDir["GLFW"] =		"$(SolutionDir)/Blu/engine/ExternalDependencies/GLFW/include"
	IncludeDir["ImGui"] =		"$(SolutionDir)/Blu/engine/ExternalDependencies/imgui"
	IncludeDir["glm"] =			"$(SolutionDir)/Blu/engine/ExternalDependencies/glm"
	IncludeDir["stb_image"] =	"$(SolutionDir)/Blu/engine/ExternalDependencies/stb_image"
	IncludeDir["entt"] =		"$(SolutionDir)/Blu/engine/ExternalDependencies/entt/include"
	IncludeDir["yaml"] =		"$(SolutionDir)/Blu/engine/ExternalDependencies/yaml/include"
	IncludeDir["ImGuizmo"] =	"$(SolutionDir)/Blu/engine/ExternalDependencies/ImGuizmo"
	IncludeDir["box2d"] =		"$(SolutionDir)/Blu/engine/ExternalDependencies/box2d/include"
	IncludeDir["assimp"] =		"$(SolutionDir)/Blu/engine/ExternalDependencies/assimp/include"
	IncludeDir["jolt"] =		"$(SolutionDir)/Blu/engine/ExternalDependencies/jolt"


	LibraryDir = {}

	Library = {}

	--Windows
	Library["WinSock"] = "Ws2_32.lib"
	Library["Winmm"] = "Winmm.lib"
	Library["Version"] = "Version.lib"
	Library["Bcrypt"] = "Bcrypt.lib"
	Library["ucrt"] = "ucrt.lib"
	
	
	Library["libm"] = "libm.lib";
	Library["libcmt"] = "libcmt.lib";
	Library["libcmtd"] = "libcmtd.lib";
	Library["libucrtd"] = "libucrtd.lib"
	
	Library["ucrt"] = "ucrt.lib";
	Library["msvcrt"] = "msvcrt.lib";
	Library["msvcrtd"] = "msvcrtd.lib";


group"Dependencies"
	include "Blu/engine/ExternalDependencies/GLFW"
	include "Blu/engine/ExternalDependencies/Glad"
	include "Blu/engine/ExternalDependencies/imgui"
	include "Blu/engine/ExternalDependencies/yaml"
	include "Blu/engine/ExternalDependencies/box2d"
	include "Blu/engine/ExternalDependencies/assimp"
	include "Blu/engine/ExternalDependencies/jolt"

	project "Box2D"
		filter "configurations:Debug"
			buildoptions { "/FS" }
		filter {}
-- Setup multiple premake files per directory so we can include them in here
group "Core"
	--include "Blu"

group "Tools"
	--include "Blu-Editor"

group "misc"
	--include "Azure"


	


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
		"%{prj.name}/engine/ExternalDependencies/ImGuizmo/ImGuizmo.cpp",
		"%{prj.name}/engine/ExternalDependencies/imgui/backends/imgui_impl_dx11.h",
		"%{prj.name}/engine/ExternalDependencies/imgui/backends/imgui_impl_dx11.cpp",
		"%{prj.name}/engine/ExternalDependencies/imgui/backends/imgui_impl_glfw.h",
		"%{prj.name}/engine/ExternalDependencies/imgui/backends/imgui_impl_glfw.cpp",
		"%{prj.name}/engine/src/Blu/Platform/DirectX11/D3D11PipelineState.h",
		"%{prj.name}/engine/src/Blu/Platform/DirectX11/D3D11PipelineState.cpp"
	}   

	includedirs
	{
		"$(SolutionDir)/Blu/engine/ExternalDependencies/spdlog/include",
		"$(SolutionDir)Blu/engine/src",
		"%{IncludeDir.GLFW}",
		"%{IncludeDir.Glad}",
		"%{IncludeDir.ImGui}",
		"$(SolutionDir)/Blu/engine/ExternalDependencies/imgui/backends",
		"%{IncludeDir.glm}",
		"%{IncludeDir.stb_image}",
		"%{IncludeDir.entt}",
		"%{IncludeDir.yaml}",
		"%{IncludeDir.ImGuizmo}",
		"%{IncludeDir.box2d}",
		"%{IncludeDir.assimp}",
		"%{IncludeDir.jolt}",



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
		"assimp",
		"JoltPhysics",
		-- DirectX 11
		"d3d11",
		"dxgi",
		"d3dcompiler",
		"dxguid",
	}
	filter {"files:Blu/engine/ExternalDependencies/ImGuizmo/*.cpp"}
		flags {"NoPCH"}

	filter {"files:Blu/engine/ExternalDependencies/imgui/backends/*.cpp"}
		flags {"NoPCH"}

	
	--buildoptions { "/wd4251" } for dll


	filter "system:windows"
		systemversion "latest"

		links 
		{
			"%{Library.WinSock}",
			"%{Library.Winmm}",
			"%{Library.Version}",
			"%{Library.Bcrypt}",
			
		}
		--"opengl32.lib"
		defines
		{
			"BLU_PLATFORM_WINDOWS",
			"BLU_BUILD_DLL",
			"GLFW_INCLUDE_NONE",
			"NOMINMAX",
			"IMGUI_DEFINE_MATH_OPERATORS",
			"ASSIMP_BUILD_NO_EXPORT",
			"JPH_DISABLE_CUSTOM_ALLOCATOR",
			--"BLU_ENABLE_ASSERTS"
		}

		

	filter "configurations:Debug"
		defines "BLU_DEBUG"
		buildoptions "/MTd"
		symbols "on"
		--linkoptions { "/NODEFAULTLIB:MSVCRT" }

	filter "configurations:Release"
		defines { "BLU_RELEASE", "NDEBUG" }
		buildoptions "/MT"
		optimize "on"

	filter "configurations:Dist"
		defines { "BLU_DIST", "NDEBUG" }
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
		"%{prj.name}/src/**.h",
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
			"BLU_PLATFORM_WINDOWS",
			"_CRT_SECURE_NO_WARNINGS"

		}

	filter "configurations:Debug"
		defines "BLU_DEBUG"
		symbols "on"
		buildoptions "/MTd"
		linkoptions { "/NODEFAULTLIB:\"MSVCRTD.lib\"" }
		runtime "Debug"
		buildoptions "/MTd"


		links
		{
			"%{Library.msvcrtd}",
		}


	filter "configurations:Release"
		defines { "BLU_RELEASE", "NDEBUG" }
		optimize "on"
		runtime "Release"
		buildoptions "/MT"
		links
		{
			"%{Library.msvcrt}",
		}

	filter "configurations:Dist"
		defines { "BLU_DIST", "NDEBUG" }
		optimize "on"








project "Blu-Editor"
	location "Blu-Editor"
	kind "ConsoleApp"
	language "C++"
	cppdialect "C++20"
	staticruntime "on" 
	dependson { "Blu" }



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
		"%{IncludeDir.ImGuizmo}",
		"%{IncludeDir.assimp}"






	}
	
	links
	{
		"Blu",
		"ImGui",
		"GLFW",
		"yaml",
		"Glad",
		"assimp"


	}
	--buildoptions { "/wd4251" } for dll

	filter "system:windows"
		systemversion "latest"

		defines
		{
			"BLU_PLATFORM_WINDOWS",
			"_CRT_SECURE_NO_WARNINGS",
			"ASSIMP_BUILD_NO_EXPORT"

		}

	filter "configurations:Debug"
		defines "BLU_DEBUG"
		symbols "on"
		runtime "Debug"
		buildoptions "/MTd"


		links
		{
			"%{Library.msvcrtd}",
			
		}

	filter "configurations:Release"
		defines { "BLU_RELEASE", "NDEBUG" }
		optimize "on"
		runtime "Release"
		buildoptions "/MT"

		links
		{
			"%{Library.msvcrt}",
		}


	filter "configurations:Dist"
		defines { "BLU_DIST", "NDEBUG" }
		optimize "on"
