project "Blu-ScriptCore"
	location "Blu-ScriptCore"
	kind "SharedLib"
	language "C#"
	dotnetframework "4.7.2"
solutiondir = path.getabsolute("..")

	targetdir (solutiondir .. "/Blu-Editor/Resources/Scripts")
	objdir (solutiondir .. "/Blu-Editor/Resources/Scripts/Intermediates")

	files
	{
		"Source/**.cs",
		"Properties/**.cs",
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

group ""

group "Tools"