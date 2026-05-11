// ============================================================================
// Renderer3D_Mesh.glsl
// Frank Luna Chapter 7 lighting — directional, point, and spot lights.
// ============================================================================

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
    vec4 worldPos = u_Model * vec4(a_Position, 1.0);
    v_FragPos     = worldPos.xyz;
    v_Normal      = u_NormalMatrix * a_Normal;
    v_TexCoord    = a_TexCoord;
    gl_Position   = u_ViewProjectionMatrix * worldPos;
}

// ============================================================================

#type fragment
#version 330 core

// ---- limits ----------------------------------------------------------------
#define MAX_DIR_LIGHTS   4
#define MAX_POINT_LIGHTS 8
#define MAX_SPOT_LIGHTS  4

// ---- structs ---------------------------------------------------------------
struct Material {
    vec3  ambient;
    vec3  diffuse;
    vec3  specular;
    float shininess;
};

// Ambient/Diffuse/Specular are already pre-multiplied by Intensity on the CPU.
struct DirectionalLight {
    vec3 Direction;   // toward light source (normalised)
    vec3 Ambient;
    vec3 Diffuse;
    vec3 Specular;
    float Intensity;  // kept for reference; already baked into the colours
};

struct PointLight {
    vec3  Position;
    vec3  Ambient;
    vec3  Diffuse;
    vec3  Specular;
    float Intensity;
    float Range;
    vec3  Att;        // (constant, linear, quadratic)
};

struct SpotLight {
    vec3  Position;
    vec3  Direction;  // normalised, points along the cone axis away from light
    vec3  Ambient;
    vec3  Diffuse;
    vec3  Specular;
    float Intensity;
    float Range;
    float InnerCutoff;  // cos(inner angle)
    float OuterCutoff;  // cos(outer angle)
    vec3  Att;
};

// ---- uniforms --------------------------------------------------------------
uniform Material        u_Material;
uniform DirectionalLight u_DirLights[MAX_DIR_LIGHTS];
uniform PointLight       u_PointLights[MAX_POINT_LIGHTS];
uniform SpotLight        u_SpotLights[MAX_SPOT_LIGHTS];
uniform int  u_NumDirLights;
uniform int  u_NumPointLights;
uniform int  u_NumSpotLights;
uniform vec3 u_ViewPos;
uniform int  u_EntityID;

// ---- outputs ---------------------------------------------------------------
layout(location = 0) out vec4 o_Color;
layout(location = 1) out int  o_EntityID;

// ---- varyings --------------------------------------------------------------
in vec3 v_FragPos;
in vec3 v_Normal;
in vec2 v_TexCoord;

// ============================================================================
// Light contribution functions
// ============================================================================

vec3 CalcDirLight(DirectionalLight L, vec3 N, vec3 V)
{
    vec3  lightDir = normalize(-L.Direction);
    vec3  R        = reflect(-lightDir, N);

    float diff = max(dot(N, lightDir), 0.0);
    float spec = pow(max(dot(V, R), 0.0), max(u_Material.shininess, 1.0));

    vec3 ambient  = L.Ambient  * u_Material.ambient;
    vec3 diffuse  = L.Diffuse  * diff * u_Material.diffuse;
    vec3 specular = L.Specular * spec * u_Material.specular;

    return ambient + diffuse + specular;
}

vec3 CalcPointLight(PointLight L, vec3 N, vec3 fragPos, vec3 V)
{
    vec3  toLight  = L.Position - fragPos;
    float d        = length(toLight);

    if (d > L.Range) return vec3(0.0);

    vec3  lightDir = toLight / d;
    vec3  R        = reflect(-lightDir, N);

    float diff = max(dot(N, lightDir), 0.0);
    float spec = pow(max(dot(V, R), 0.0), max(u_Material.shininess, 1.0));

    float att  = 1.0 / dot(L.Att, vec3(1.0, d, d * d));

    vec3 ambient  = L.Ambient  * u_Material.ambient;
    vec3 diffuse  = L.Diffuse  * diff * u_Material.diffuse;
    vec3 specular = L.Specular * spec * u_Material.specular;

    return (ambient + diffuse + specular) * att;
}

vec3 CalcSpotLight(SpotLight L, vec3 N, vec3 fragPos, vec3 V)
{
    vec3  toLight  = L.Position - fragPos;
    float d        = length(toLight);

    if (d > L.Range) return vec3(0.0);

    vec3  lightDir = toLight / d;
    vec3  R        = reflect(-lightDir, N);

    float diff = max(dot(N, lightDir), 0.0);
    float spec = pow(max(dot(V, R), 0.0), max(u_Material.shininess, 1.0));

    float att  = 1.0 / dot(L.Att, vec3(1.0, d, d * d));

    // Smooth cone falloff
    float theta      = dot(lightDir, normalize(-L.Direction));
    float epsilon    = L.InnerCutoff - L.OuterCutoff;
    float spotFactor = clamp((theta - L.OuterCutoff) / epsilon, 0.0, 1.0);

    vec3 ambient  = L.Ambient  * u_Material.ambient;
    vec3 diffuse  = L.Diffuse  * diff * u_Material.diffuse;
    vec3 specular = L.Specular * spec * u_Material.specular;

    return (ambient + (diffuse + specular) * spotFactor) * att;
}

// ============================================================================
void main()
{
    vec3 N = normalize(v_Normal);
    vec3 V = normalize(u_ViewPos - v_FragPos);

    vec3 result = vec3(0.0);

    // No fallback for zero lights — objects are black when unlit (correct behaviour).
    for (int i = 0; i < u_NumDirLights   && i < MAX_DIR_LIGHTS;   ++i)
        result += CalcDirLight(u_DirLights[i], N, V);

    for (int i = 0; i < u_NumPointLights && i < MAX_POINT_LIGHTS; ++i)
        result += CalcPointLight(u_PointLights[i], N, v_FragPos, V);

    for (int i = 0; i < u_NumSpotLights  && i < MAX_SPOT_LIGHTS;  ++i)
        result += CalcSpotLight(u_SpotLights[i], N, v_FragPos, V);

    o_Color    = vec4(result, 1.0);
    o_EntityID = u_EntityID;
}
