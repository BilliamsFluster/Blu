#type vertex
#version 330 core

layout(location = 0) in vec4 a_Color;
layout(location = 1) in vec3 a_Position;
layout(location = 2) in vec2 a_TexCoord;
layout(location = 3) in float a_TexIndex;
layout(location = 4) in float a_TilingFactor;
layout(location = 5) in float a_Thickness;
layout(location = 6) in int a_EntityID;
layout(location = 7) in vec3 a_Normal;

uniform mat4 u_ViewProjectionMatrix;

out vec2 v_TexCoord;
out vec4 v_Color;
flat out float v_TexIndex;
out float v_TilingFactor;
flat out float v_Thickness;
flat out int v_EntityID;
out vec3 v_Normal;
out vec3 v_FragPosition;

void main()
{
    v_Color        = a_Color;
    v_Normal       = a_Normal;
    v_TexCoord     = a_TexCoord;
    v_TexIndex     = a_TexIndex;
    v_TilingFactor = a_TilingFactor;
    v_Thickness    = a_Thickness;
    v_EntityID     = a_EntityID;
    v_FragPosition = a_Position;

    gl_Position = u_ViewProjectionMatrix * vec4(a_Position, 1.0);
}


#type fragment
#version 330 core

#define MAX_LIGHTS 8

struct Material {
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    float shininess;
};

struct Light {
    vec3 position;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

layout(location = 0) out vec4 o_color;
layout(location = 1) out int o_EntityID;

uniform Material o_Material;
uniform Light    u_Lights[MAX_LIGHTS];
uniform int      u_NumLights;
uniform vec3     u_ViewPos;

uniform sampler2D u_Textures[32];

in vec2 v_TexCoord;
in vec4 v_Color;
flat in float v_TexIndex;
in float v_TilingFactor;
flat in float v_Thickness;
flat in int v_EntityID;
in vec3 v_Normal;
in vec3 v_FragPosition;

void main()
{
    vec3 norm    = normalize(v_Normal);
    vec3 viewDir = normalize(u_ViewPos - v_FragPosition);

    vec3 lightResult = vec3(0.0);

    if (u_NumLights == 0)
    {
        // No lights in scene — render unlit
        lightResult = vec3(1.0);
    }
    else
    {
        for (int i = 0; i < u_NumLights && i < MAX_LIGHTS; i++)
        {
            vec3 lightDir   = normalize(u_Lights[i].position - v_FragPosition);
            vec3 reflectDir = reflect(-lightDir, norm);

            vec3 ambient  = u_Lights[i].ambient  * o_Material.ambient;

            float diff    = max(dot(norm, lightDir), 0.0);
            vec3 diffuse  = u_Lights[i].diffuse  * (diff * o_Material.diffuse);

            float spec    = pow(max(dot(viewDir, reflectDir), 0.0), max(o_Material.shininess, 1.0));
            vec3 specular = u_Lights[i].specular * (spec * o_Material.specular);

            lightResult += ambient + diffuse + specular;
        }
    }

    vec4 texColor  = texture(u_Textures[int(v_TexIndex)], v_TexCoord * v_TilingFactor);
    o_color    = texColor * vec4(lightResult, 1.0) * v_Color;
    o_EntityID = v_EntityID;
}
