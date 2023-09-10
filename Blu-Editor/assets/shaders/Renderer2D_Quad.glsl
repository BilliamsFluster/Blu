#type vertex
#version 330 core
			
layout(location = 0) in vec4 a_Color;
layout(location = 1) in vec3 a_Position;
layout(location = 2) in vec2 a_TexCoord;
layout(location = 3) in float a_TexIndex;
layout(location = 4) in float a_TilingFactor;
layout(location = 5) in float a_Thickness;
layout(location = 6) in int a_EntityID;
			
uniform mat4 u_ViewProjectionMatrix;

out vec2 v_TexCoord;
out vec4 v_Color;
flat out float v_TexIndex;
out float v_TilingFactor;
flat out float v_Thickness;
flat out int v_EntityID;
out vec3 v_Position;

// Define the PointLight
struct PointLight {
    vec4 Color;
    float Intensity;
    float Radius;
    vec3 Position;
    vec3 AmbientColor;
    vec3 DiffuseColor;
    vec3 SpecularColor;
    float Shininess;
};

//Uniform array of PointLight structs
uniform PointLight u_PointLights[32];



void main()
{
	
    v_Color = a_Color;
    v_Position = a_Position; 

	v_TexCoord = a_TexCoord;
	v_TexIndex = a_TexIndex;
	v_TilingFactor = a_TilingFactor;
	v_Thickness = a_Thickness;
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
in vec3 v_Position;
flat in float v_TexIndex;
in float v_TilingFactor;
flat in float v_Thickness;
flat in int v_EntityID;



void main()
{
	/*vec3 lightingColor = vec3(0.0); // Initialize with ambient light

    for (int i = 0; i < 32; ++i) {
        // Calculate the vector from the vertex to the light position
        vec3 lightDir = u_PointLights[i].Position - v_Position;
        
        // Calculate the distance between the vertex and the light
        float distance = length(lightDir);
        
        // Normalize the light direction
        lightDir = normalize(lightDir);
        
        // Calculate diffuse lighting (Lambertian reflection)
        float diffuse = max(dot(lightDir, a_Normal), 0.0);
        
        // Calculate attenuation based on distance
        float attenuation = 1.0 / (u_PointLights[i].Constant + u_PointLights[i].Linear * distance + u_PointLights[i].Quadratic * (distance * distance));
        
        // Calculate specular reflection (Phong reflection)
        vec3 viewDir = normalize(-v_Position); // View direction
        vec3 reflectDir = reflect(-lightDir, a_Normal); // Reflected light direction
        float specular = pow(max(dot(viewDir, reflectDir), 0.0), u_PointLights[i].Shininess);
        
        // Combine ambient, diffuse, and specular lighting
        vec3 ambientColor = u_PointLights[i].AmbientColor;
        vec3 diffuseColor = u_PointLights[i].DiffuseColor;
        vec3 specularColor = u_PointLights[i].SpecularColor;
        
        vec3 light = (ambientColor + (diffuseColor * diffuse + specularColor * specular)) * attenuation;
        
        // Add the light contribution to the final color
        lightingColor += light * v_Color.rgb;
    }*/
    // Set the final color to the modified lighting color
	o_color = texture(u_Textures[int(v_TexIndex)], v_TexCoord * v_TilingFactor) * v_Color;
	o_EntityID = v_EntityID;
	
}