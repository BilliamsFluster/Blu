#type vertex
#version 330 core
			
layout(location = 0) in vec4 a_Color;
layout(location = 1) in vec3 a_Position;
layout(location = 2) in vec2 a_TexCoord;
layout(location = 3) in float a_TexIndex;
layout(location = 4) in float a_TilingFactor;
layout(location = 5) in int a_EntityID;
			
uniform mat4 u_ViewProjectionMatrix;

out vec2 v_TexCoord;
out vec4 v_Color;
flat out float v_TexIndex;
out float v_TilingFactor;
flat out int v_EntityID;
void main()
{
	v_Color = a_Color;
	v_TexCoord = a_TexCoord;
	v_TexIndex = a_TexIndex;
	v_TilingFactor = a_TilingFactor;
	v_EntityID = a_EntityID;
	gl_Position = u_ViewProjectionMatrix * vec4(a_Position, 1.0);	
}


#type fragment
#version 330 core
			
layout(location = 0) out vec4 o_color;
layout(location = 1) out int o_EntityID;
			
uniform sampler2D u_Textures[32];

in vec2 v_TexCoord;
in vec4 v_Color;
flat in float v_TexIndex;
in float v_TilingFactor;
flat in int v_EntityID;



void main()
{
	
	o_color = texture(u_Textures[int(v_TexIndex)], v_TexCoord * v_TilingFactor) * v_Color;
	o_EntityID = v_EntityID;

	
}