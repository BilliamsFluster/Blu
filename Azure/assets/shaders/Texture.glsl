#type vertex
#version 330 core
			
layout(location = 0) in vec4 a_Color;
layout(location = 1) in vec3 a_Position;
layout(location = 2) in vec2 a_TexCoord;
			
uniform mat4 u_ViewProjectionMatrix;

out vec2 v_TexCoord;
out vec4 v_Color;
void main()
{
	v_Color = a_Color;
	v_TexCoord = a_TexCoord;
	gl_Position = u_ViewProjectionMatrix * vec4(a_Position, 1.0);	
}


#type fragment
#version 330 core
			
layout(location = 0) out vec4 o_color;
			
uniform sampler2D u_Texture;
uniform vec4 u_Color;
uniform float u_TilingFactor;

in vec2 v_TexCoord;
in vec4 v_Color;


void main()
{
	//o_color = texture(u_Texture, v_TexCoord * u_TilingFactor) * u_Color;
	o_color = v_Color;
}