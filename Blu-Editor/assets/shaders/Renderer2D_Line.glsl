#type vertex
#version 330 core
			
layout(location = 0) in vec4 a_Color;
layout(location = 1) in vec3 a_Position;
layout(location = 2) in float a_Thickness;
layout(location = 3) in int a_EntityID;
			
uniform mat4 u_ViewProjectionMatrix;

out vec4 v_Color;
out vec3 v_Position;
out float v_Thickness;
flat out int v_EntityID;
void main()
{
	v_Color = a_Color;
	v_Position = a_Position;
	v_Thickness = a_Thickness;
	v_EntityID = a_EntityID;

	gl_Position = u_ViewProjectionMatrix * vec4(a_Position, 1.0);	
}


#type fragment
#version 330 core
			
layout(location = 0) out vec4 o_color;
layout(location = 1) out int o_EntityID;
			
uniform sampler2D u_Textures[32];

in vec4 v_Color;
in vec3 v_Position;
in float v_Thickness;
flat in int v_EntityID;



void main()
{

	o_color = v_Color;
	o_EntityID = v_EntityID;
	
}