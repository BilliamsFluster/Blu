#type vertex
#version 330 core
			
layout(location = 0) in vec4 a_Color;
layout(location = 1) in vec3 a_Position;
layout(location = 2) in vec2 a_TexCoord;
layout(location = 3) in float a_TexIndex;
layout(location = 4) in float a_TilingFactor;
layout(location = 5) in float a_Thickness;
layout(location = 6) in int a_EntityID;

layout(location = 7) in vec3 a_LightColor;
layout(location = 8) in vec3 a_Normal;
layout(location = 9) in vec3 a_LightPosition;
			
uniform mat4 u_ViewProjectionMatrix;


out vec2 v_TexCoord;
out vec4 v_Color;
flat out float v_TexIndex;
out float v_TilingFactor;
flat out float v_Thickness;
flat out int v_EntityID;
out vec3 v_Position;

out vec3 v_LightColor;
out vec3 v_Normal;
out vec3 v_LightPosition;
out vec3 v_FragPosition;

void main()
{
	 // Assuming you have a model matrix, even if it's an identity matrix for quads
    mat4 model = mat4(1.0); // Identity matrix, replace if you have a different model matrix

    v_Color = a_Color;
    v_Position = a_Position; 
	
	v_LightColor = a_LightColor;
	v_LightPosition = a_LightPosition;
	//v_LightPosition = vec3(10,-2,20);
	v_Normal = a_Normal;

	v_TexCoord = a_TexCoord;
	v_TexIndex = a_TexIndex;
	v_TilingFactor = a_TilingFactor;
	v_Thickness = a_Thickness;
	v_EntityID = a_EntityID;

	
    


	gl_Position = u_ViewProjectionMatrix * vec4(a_Position, 1.0);
	v_FragPosition = vec3(model * vec4(a_Position, 1.0));

}


#type fragment
#version 330 core
			
layout(location = 0) out vec4 o_color;
layout(location = 1) out int o_EntityID;

			
uniform sampler2D u_Textures[32];
in vec2 v_TexCoord;
in vec4 v_Color;
in vec3 v_Position;
flat in float v_TexIndex;
in float v_TilingFactor;
flat in float v_Thickness;
flat in int v_EntityID;

in vec3 v_LightColor;
in vec3 v_LightPosition;
in vec3 v_Normal;
in vec3 v_FragPosition;


void main()
{
	
    // Set the final color to the modified lighting color
	float ambientStrength = 0.1;
	float specularStrength = 0.5;
	
	vec3 ambient = ambientStrength * v_LightColor;
	
	vec3 norm = normalize(v_Normal);
	vec3 lightDir = normalize(v_LightPosition - v_FragPosition); 
	float diff = max(dot(norm, lightDir), 0.0);
	vec3 diffuse = diff * v_LightColor;

	vec3 viewDir = normalize(v_Position - v_FragPosition);
	vec3 reflectDir = reflect(-lightDir, norm); 

	float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
	vec3 specular = specularStrength * spec * v_LightColor;  

	// Combine ambient and diffuse lighting
    vec3 lightResult = ambient + diffuse + specular;

    // Apply the lighting to the color
    vec4 finalColor = vec4(lightResult, 1.0) * v_Color;


	o_color = texture(u_Textures[int(v_TexIndex)], v_TexCoord * v_TilingFactor) * finalColor; 
	o_EntityID = v_EntityID;
	
}