solutiondir = path.getabsolute("..")

project "Blu-ScriptCore"
    location (solutiondir .. "/Blu-ScriptCore")
    kind "SharedLib"
    language "C#"
    dotnetframework "4.7.2"

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
