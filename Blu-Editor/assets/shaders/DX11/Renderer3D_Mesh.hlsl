// Renderer3D_Mesh.hlsl
// Frank Luna Chapter 7 lighting model — directional, point, and spot lights.
//
// Constant buffer / semantic names must match the engine's reflection map.
// All matrices are uploaded column-major (raw GLM) and HLSL cbuffer default
// is also column-major, so mul(M, v) gives the correct M*v.

// =============================================================================
// Constant buffers
// =============================================================================

cbuffer PerFrame : register(b0)
{
    float4x4 u_ViewProjectionMatrix;
    float3   u_ViewPos;
    float    _pad0;
};

cbuffer PerObject : register(b1)
{
    float4x4 u_Model;
    // float3x3 in a cbuffer packs each column as float4 (16 bytes each = 48 bytes total),
    // but GLM mat3 is 36 bytes (no padding).  Uploading a GLM mat3 via memcpy corrupts
    // columns 1 and 2.  Fix: store as three explicit float3+float4 rows (each 16 bytes).
    float3 u_NormalCol0;  float _n0;   // column 0 of the normal matrix
    float3 u_NormalCol1;  float _n1;   // column 1
    float3 u_NormalCol2;  float _n2;   // column 2
    int    u_EntityID;    float3 _pad2; // 16 bytes
};

// ─── Material ─────────────────────────────────────────────────────────────────
struct Material
{
    float3 ambient;   float _p0;
    float3 diffuse;   float _p1;
    float3 specular;  float shininess;  // shininess in .w
};

cbuffer MaterialData : register(b2)
{
    Material u_Material;
};

// =============================================================================
// Light structs
// =============================================================================

// ─── Directional light (sun / sky) ────────────────────────────────────────────
// No position, no attenuation.  Direction points *toward* the light source.
struct DirectionalLight
{
    float3 Direction;  float _p0;      // 16
    float3 Ambient;    float Intensity; // 16
    float3 Diffuse;    float _p1;      // 16
    float3 Specular;   float _p2;      // 16
    // total: 64 bytes
};

// ─── Point light with attenuation ─────────────────────────────────────────────
// att = 1 / dot(Att, float3(1, d, d*d))   where d = distance to fragment
struct PointLight
{
    float3 Position;  float Range;     // 16
    float3 Ambient;   float Intensity; // 16
    float3 Diffuse;   float _p0;       // 16
    float3 Specular;  float _p1;       // 16
    float3 Att;       float _p2;       // 16  Att = (constant, linear, quadratic)
    // total: 80 bytes
};

// ─── Spot light ───────────────────────────────────────────────────────────────
// Smooth inner/outer cone falloff.
// InnerCutoff = cos(innerAngle),  OuterCutoff = cos(outerAngle)
// spotFactor = saturate((cos(theta) - OuterCutoff) / (InnerCutoff - OuterCutoff))
struct SpotLight
{
    float3 Position;     float Range;       // 16
    float3 Direction;    float Intensity;   // 16
    float3 Ambient;      float _p0;         // 16
    float3 Diffuse;      float _p1;         // 16
    float3 Specular;     float _p2;         // 16
    float3 Att;          float InnerCutoff; // 16
    float  OuterCutoff;  float3 _p3;        // 16
    // total: 112 bytes
};

cbuffer LightData : register(b3)
{
    DirectionalLight u_DirLights[4];
    PointLight       u_PointLights[8];
    SpotLight        u_SpotLights[4];
    int  u_NumDirLights;
    int  u_NumPointLights;
    int  u_NumSpotLights;
    float _padL;
};

// =============================================================================
// Vertex shader
// =============================================================================

#type vertex

struct VS_IN
{
    float3 a_Position : a_Position;
    float3 a_Normal   : a_Normal;
    float2 a_TexCoord : a_TexCoord;
};

struct VS_OUT
{
    float4 Position  : SV_Position;
    float3 FragPos   : FRAGPOS;
    float3 Normal    : NORMAL;
    float2 TexCoord  : TEXCOORD;
};

VS_OUT main(VS_IN IN)
{
    VS_OUT OUT;
    float4 worldPos  = mul(u_Model, float4(IN.a_Position, 1.0));
    OUT.FragPos  = worldPos.xyz;
    OUT.Position = mul(u_ViewProjectionMatrix, worldPos);
    // Reconstruct the normal matrix from its three explicit columns and apply it.
    // mul(M, v) = v.x*col0 + v.y*col1 + v.z*col2  (column-major multiply)
    float3 transformedNormal = IN.a_Normal.x * u_NormalCol0
                             + IN.a_Normal.y * u_NormalCol1
                             + IN.a_Normal.z * u_NormalCol2;
    OUT.Normal   = normalize(transformedNormal);
    OUT.TexCoord = IN.a_TexCoord;
    return OUT;
}

// =============================================================================
// Pixel shader
// =============================================================================

#type pixel

struct PS_IN
{
    float4 Position : SV_Position;
    float3 FragPos  : FRAGPOS;
    float3 Normal   : NORMAL;
    float2 TexCoord : TEXCOORD;
};

struct PS_OUT
{
    float4 Color    : SV_Target0;
    int    EntityID : SV_Target1;
};

// ─── Directional light contribution ──────────────────────────────────────────
float3 CalcDirLight(DirectionalLight L, float3 N, float3 V)
{
    float3 lightDir = normalize(-L.Direction);
    float3 R        = reflect(-lightDir, N);

    float  diff = max(dot(N, lightDir), 0.0f);
    float  spec = pow(max(dot(V, R), 0.0f), max(u_Material.shininess, 1.0f));

    float3 ambient  = L.Ambient  * u_Material.ambient;
    float3 diffuse  = L.Diffuse  * diff * u_Material.diffuse;
    float3 specular = L.Specular * spec * u_Material.specular;

    return ambient + diffuse + specular;
}

// ─── Point light with quadratic attenuation ───────────────────────────────────
float3 CalcPointLight(PointLight L, float3 N, float3 fragPos, float3 V)
{
    float3 toLight = L.Position - fragPos;
    float  d       = length(toLight);

    // Hard cutoff at range — avoids tiny contributions from distant lights
    if (d > L.Range) return float3(0.0f, 0.0f, 0.0f);

    float3 lightDir = toLight / d;   // normalize without a second sqrt
    float3 R        = reflect(-lightDir, N);

    float  diff = max(dot(N, lightDir), 0.0f);
    float  spec = pow(max(dot(V, R), 0.0f), max(u_Material.shininess, 1.0f));

    // att = 1 / (c + l*d + q*d²)
    float  att  = 1.0f / dot(L.Att, float3(1.0f, d, d * d));

    float3 ambient  = L.Ambient  * u_Material.ambient;
    float3 diffuse  = L.Diffuse  * diff * u_Material.diffuse;
    float3 specular = L.Specular * spec * u_Material.specular;

    return (ambient + diffuse + specular) * att;
}

// ─── Spot light with smooth inner/outer cone ──────────────────────────────────
float3 CalcSpotLight(SpotLight L, float3 N, float3 fragPos, float3 V)
{
    float3 toLight = L.Position - fragPos;
    float  d       = length(toLight);

    if (d > L.Range) return float3(0.0f, 0.0f, 0.0f);

    float3 lightDir = toLight / d;
    float3 R        = reflect(-lightDir, N);

    float  diff = max(dot(N, lightDir), 0.0f);
    float  spec = pow(max(dot(V, R), 0.0f), max(u_Material.shininess, 1.0f));

    float  att  = 1.0f / dot(L.Att, float3(1.0f, d, d * d));

    // Smooth cone: theta is cosine of angle between light-to-fragment and spot direction
    float  theta      = dot(lightDir, normalize(-L.Direction));
    float  epsilon    = L.InnerCutoff - L.OuterCutoff;   // both are cosines; inner > outer
    float  spotFactor = saturate((theta - L.OuterCutoff) / epsilon);

    float3 ambient  = L.Ambient  * u_Material.ambient;
    float3 diffuse  = L.Diffuse  * diff * u_Material.diffuse;
    float3 specular = L.Specular * spec * u_Material.specular;

    // Ambient is not modulated by the spot cone so the area outside the cone
    // still receives a small ambient contribution (same as point lights)
    return (ambient + (diffuse + specular) * spotFactor) * att;
}

// ─── Main pixel shader ────────────────────────────────────────────────────────
PS_OUT main(PS_IN IN)
{
    PS_OUT OUT;

    float3 N = normalize(IN.Normal);
    float3 V = normalize(u_ViewPos - IN.FragPos);

    float3 result = float3(0.0f, 0.0f, 0.0f);

    // No fallback for zero lights — objects are black when unlit (correct behaviour).
    for (int i = 0; i < u_NumDirLights;   ++i) result += CalcDirLight  (u_DirLights[i],   N, V);
    for (int i = 0; i < u_NumPointLights; ++i) result += CalcPointLight(u_PointLights[i], N, IN.FragPos, V);
    for (int i = 0; i < u_NumSpotLights;  ++i) result += CalcSpotLight (u_SpotLights[i],  N, IN.FragPos, V);

    OUT.Color    = float4(result, 1.0f);
    OUT.EntityID = u_EntityID;
    return OUT;
}
