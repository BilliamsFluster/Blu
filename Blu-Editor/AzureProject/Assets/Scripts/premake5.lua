
solutiondir = path.getabsolute("..")

workspace "AzureProject"
	architecture "x64"
	startproject "Azure-Project"
	
	configurations
	{
		"Debug",
		"Release",
		"Dist"
	}
	outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"


project "Azure-Project"
    location (solutiondir .. "/Blu-ScriptCore")
    kind "SharedLib"
    language "C#"
    dotnetframework "4.7.2"

    targetdir ("Binaries")
    objdir ("Intermediates")

	files
	{
		"Source/**.cs",
		"Properties/**.cs",
	}
	links
	{
		"Blu-ScriptCore"
	}

	filter "configurations:Debug"
		optimize "Off"
		symbols "Default"

	filter "configurations:Release"
		optimize "On"
		symbols "Default"

	filter "configurations:Dist"
		optimize "Full"
		symbols "Off"

group "Blu"
	include "../../../../Blu-ScriptCore"

group ""