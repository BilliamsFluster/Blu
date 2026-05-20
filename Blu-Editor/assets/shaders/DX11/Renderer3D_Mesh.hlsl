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
    int      u_HasAlbedoTexture;
    int      u_HasShadowMap;
};

cbuffer PerObject : register(b1)
{
    float4x4 u_Model;          // 64 bytes
    float3x3 u_NormalMatrix;   // 48 bytes (3 float4 columns, column-major)
    int    u_EntityID;         float3 _pad2; // 16 bytes
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

// ─── Shadow map ───────────────────────────────────────────────────────────────
cbuffer ShadowData : register(b4)
{
    float4x4 u_LightVP;
};

// ─── Fog + aerial perspective ─────────────────────────────────────────────────
cbuffer FogData : register(b5)
{
    float3 u_FogColor;           float  u_FogDensity;       // 16
    float  u_FogHeightStart;     float  u_FogHeightDensity; float2 _fogPad0; // 16
    float3 u_AerialColor;        float  u_AerialStrength;   // 16 — sky colour bleed at horizon
    int    u_FogEnabled;         float3 _fogPad1;           // 16
};

Texture2D                u_ShadowMap      : register(t1);
SamplerComparisonState   u_ShadowSampler  : register(s1);

// ─── PCF shadow factor ────────────────────────────────────────────────────────
float CalcShadowFactor(float4 fragPosLightSpace)
{
    float3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;

    float currentDepth = projCoords.z;
    if (currentDepth > 1.0) return 1.0;

    float2 texelSize = 1.0 / 2048.0;
    float shadow = 0.0;
    [unroll]
    for (int x = -1; x <= 1; ++x)
        [unroll]
        for (int y = -1; y <= 1; ++y)
            shadow += u_ShadowMap.SampleCmpLevelZero(u_ShadowSampler, projCoords.xy + float2(x, y) * texelSize, currentDepth);
    shadow /= 9.0;
    return shadow;
}

Texture2D    u_AlbedoTexture : register(t0);
SamplerState u_AlbedoSampler : register(s0);

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
    float4 FragPosLightSpace : LIGHTSPACE;
};

VS_OUT main(VS_IN IN)
{
    VS_OUT OUT;
    float4 worldPos  = mul(u_Model, float4(IN.a_Position, 1.0));
    OUT.FragPos  = worldPos.xyz;
    OUT.Position = mul(u_ViewProjectionMatrix, worldPos);
    OUT.Normal   = normalize(mul(u_NormalMatrix, IN.a_Normal));
    OUT.TexCoord = IN.a_TexCoord;
    OUT.FragPosLightSpace = mul(u_LightVP, worldPos);
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
    float4 FragPosLightSpace : LIGHTSPACE;
};

struct PS_OUT
{
    float4 Color    : SV_Target0;
    int    EntityID : SV_Target1;
};

// ─── Directional light contribution ──────────────────────────────────────────
float3 CalcDirLight(DirectionalLight L, float3 N, float3 V, float shadowFactor)
{
    float3 lightDir = normalize(-L.Direction);
    float3 R        = reflect(-lightDir, N);

    float  diff = max(dot(N, lightDir), 0.0f);
    float  spec = pow(max(dot(V, R), 0.0f), max(u_Material.shininess, 1.0f));

    float3 ambient  = L.Ambient  * u_Material.ambient;
    float3 diffuse  = L.Diffuse  * diff * u_Material.diffuse * shadowFactor;
    float3 specular = L.Specular * spec * u_Material.specular * shadowFactor;

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

    float shadowFactor = u_HasShadowMap ? CalcShadowFactor(IN.FragPosLightSpace) : 1.0;

    for (int i = 0; i < u_NumDirLights;   ++i) result += CalcDirLight  (u_DirLights[i],   N, V, shadowFactor);
    for (int i = 0; i < u_NumPointLights; ++i) result += CalcPointLight(u_PointLights[i], N, IN.FragPos, V);
    for (int i = 0; i < u_NumSpotLights;  ++i) result += CalcSpotLight (u_SpotLights[i],  N, IN.FragPos, V);

    if (u_HasAlbedoTexture)
    {
        float4 texColor = u_AlbedoTexture.Sample(u_AlbedoSampler, IN.TexCoord);
        result *= texColor.rgb;
    }

    // ─── Fog + aerial perspective ─────────────────────────────────────────────
    if (u_FogEnabled)
    {
        float  dist       = length(u_ViewPos - IN.FragPos);
        float  fogFactor  = exp(-u_FogDensity * dist);

        // Height-based density: fog thins above HeightStart
        if (u_FogHeightDensity > 0.0f)
        {
            float heightAbove = max(0.0f, IN.FragPos.y - u_FogHeightStart);
            fogFactor *= exp(-heightAbove * u_FogHeightDensity);
        }
        fogFactor = saturate(fogFactor);

        // Aerial perspective: blend fog colour toward the sky horizon colour
        // as the view direction flattens — eliminates the white horizon band.
        float3 viewDir      = normalize(IN.FragPos - u_ViewPos);
        float  horizonBlend = saturate(1.0f - abs(viewDir.y)); // 1 at horizon, 0 overhead
        float3 finalFog     = lerp(u_FogColor, u_AerialColor,
                                   u_AerialStrength * horizonBlend);

        result = lerp(finalFog, result, fogFactor);
    }

    OUT.Color    = float4(result, 1.0f);
    OUT.EntityID = u_EntityID;
    return OUT;
}
