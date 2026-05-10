#type vertex
#version 330 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoord;

uniform mat4 u_ViewProjectionMatrix;
uniform mat4 u_Model;
uniform mat3 u_NormalMatrix;

out vec3 v_FragPos;
out vec3 v_Normal;
out vec2 v_TexCoord;

void main()
{
    vec4 worldPos  = u_Model * vec4(a_Position, 1.0);
    v_FragPos      = vec3(worldPos);
    v_Normal       = u_NormalMatrix * a_Normal;
    v_TexCoord     = a_TexCoord;

    gl_Position = u_ViewProjectionMatrix * worldPos;
}


#type fragment
#version 330 core

#define MAX_LIGHTS 8

struct Material {
    vec3  ambient;
    vec3  diffuse;
    vec3  specular;
    float shininess;
};

struct Light {
    vec3 position;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

layout(location = 0) out vec4 o_Color;
layout(location = 1) out int  o_EntityID;

uniform Material u_Material;
uniform Light    u_Lights[MAX_LIGHTS];
uniform int      u_NumLights;
uniform vec3     u_ViewPos;
uniform int      u_EntityID;

in vec3 v_FragPos;
in vec3 v_Normal;
in vec2 v_TexCoord;

void main()
{
    vec3 norm    = normalize(v_Normal);
    vec3 viewDir = normalize(u_ViewPos - v_FragPos);

    vec3 result = vec3(0.0);

    if (u_NumLights == 0)
    {
        result = u_Material.diffuse;
    }
    else
    {
        for (int i = 0; i < u_NumLights && i < MAX_LIGHTS; i++)
        {
            vec3 lightDir   = normalize(u_Lights[i].position - v_FragPos);
            vec3 reflectDir = reflect(-lightDir, norm);

            vec3 ambient  = u_Lights[i].ambient  * u_Material.ambient;

            float diff    = max(dot(norm, lightDir), 0.0);
            vec3 diffuse  = u_Lights[i].diffuse  * (diff * u_Material.diffuse);

            float spec    = pow(max(dot(viewDir, reflectDir), 0.0), max(u_Material.shininess, 1.0));
            vec3 specular = u_Lights[i].specular * (spec * u_Material.specular);

            result += ambient + diffuse + specular;
        }
    }

    o_Color    = vec4(result, 1.0);
    o_EntityID = u_EntityID;
}
