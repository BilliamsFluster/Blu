#type vertex
#version 330 core
			
layout(location = 0) in vec4 a_Color;
layout(location = 1) in vec3 a_WorldPosition;
layout(location = 2) in vec3 a_LocalPosition;
layout(location = 3) in float a_Fade;
layout(location = 4) in float a_Thickness;
layout(location = 5) in int a_EntityID;
			
uniform mat4 u_ViewProjectionMatrix;

out vec3 v_LocalPosition;
out float v_Fade;
out vec4 v_Color;
flat out float v_Thickness;
flat out int v_EntityID;


void main()
{
	v_Color = a_Color;
	v_Fade = a_Fade;
	v_Thickness = a_Thickness;
	v_LocalPosition = a_LocalPosition;
	v_EntityID = a_EntityID;
	gl_Position = u_ViewProjectionMatrix * vec4(a_WorldPosition, 1.0);	
}


#type fragment
#version 330 core
			
layout(location = 0) out vec4 o_Color;
layout(location = 1) out int o_EntityID;
			


in vec3 v_LocalPosition;
in float v_Fade;
in vec4 v_Color;
flat in float v_Thickness;
flat in int v_EntityID;




void main()
{
	float distance = 1.0 - length(v_LocalPosition);
    
    float circleAlpha = smoothstep(0.0, v_Fade, distance);
    circleAlpha *= smoothstep(v_Thickness + v_Fade, v_Thickness, distance);
	
	if(circleAlpha == 0.0f)
		discard;
    
    o_Color = v_Color;
    o_Color.a *= circleAlpha;
	
	
	o_EntityID = v_EntityID;

	
	
}