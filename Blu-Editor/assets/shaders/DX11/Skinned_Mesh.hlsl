// Skinned_Mesh.hlsl
// PBR-lit skeletal mesh shader.
// Bone matrices live in BoneData (b6): up to 128 bones.
// Vertex skinning: blended across up to 4 bones per vertex.
// All other cbuffer / light / fog / shadow registers mirror PBR_Mesh.hlsl.

#pragma pack_matrix(column_major)

// =============================================================================
// Constant buffers
// =============================================================================

cbuffer PerFrame : register(b0)
{
    float4x4 u_ViewProjectionMatrix;
    float3   u_ViewPos;       int u_HasShadowMap;
    float3   u_FogColor;      float u_FogDensity;
    float    u_FogHeightStart; float u_FogHeightDensity;
    int      u_FogEnabled;    float u_AerialStrength;
    float3   u_AerialColor;   float _fogPad;
};

cbuffer PerObject : register(b1)
{
    float4x4 u_Model;
    float3x3 u_NormalMatrix;
    int      u_EntityID;  float3 _padPerObj;
};

cbuffer MaterialData : register(b2)
{
    float4 u_AlbedoColor;
    float  u_Metallic;    float u_Roughness;
    float  u_AO;          float u_EmissiveStrength;
    float3 u_EmissiveColor; float u_AlphaCutoff;
    int    u_ShadingModel;  int  u_HasAlbedoMap;
    int    u_HasNormalMap;  int  u_HasMetallicRoughnessMap;
    int    u_HasAOMap;      int  u_HasEmissiveMap;
    int2   _matPad;
};

struct DirectionalLight
{
    float3 Direction; float _p0;
    float3 Ambient;   float Intensity;
    float3 Diffuse;   float _p1;
    float3 Specular;  float _p2;
};

struct PointLight
{
    float3 Position; float Range;
    float3 Ambient;  float Intensity;
    float3 Diffuse;  float _p0;
    float3 Specular; float _p1;
    float3 Att;      float _p2;
};

struct SpotLight
{
    float3 Position;    float Range;
    float3 Direction;   float Intensity;
    float3 Ambient;     float _p0;
    float3 Diffuse;     float _p1;
    float3 Specular;    float _p2;
    float3 Att;         float InnerCutoff;
    float  OuterCutoff; float3 _p3;
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

cbuffer ShadowData : register(b4)
{
    float4x4 u_LightVPs[3];
    float3   u_CascadeSplits;
    float    u_ShadowMapSize;
};

cbuffer BoneData : register(b6)
{
    float4x4 u_Bones[128]; // 128 * 64 = 8 192 bytes
};

cbuffer IBLData : register(b7)
{
    int    u_IBLEnabled;
    float  u_IBLStrength;
    int    u_IBLMipLevels;
    float  _iblPad;
};

// =============================================================================
// Textures
// =============================================================================

Texture2D    u_AlbedoTexture : register(t0);
Texture2D    u_NormalTexture : register(t1);
Texture2DArray u_ShadowMapArray : register(t5);
TextureCube  u_IrradianceMap  : register(t6);
TextureCube  u_PrefilteredEnv : register(t7);
Texture2D    u_BRDFLUT        : register(t8);
SamplerState   u_LinearSampler  : register(s0);
SamplerComparisonState u_ShadowSampler : register(s1);

// =============================================================================
// PCF Shadow
// =============================================================================

float CalcCSMShadow(float3 fragPos, float viewDepth)
{
    int cascade = 2;
    if      (viewDepth < u_CascadeSplits.x) cascade = 0;
    else if (viewDepth < u_CascadeSplits.y) cascade = 1;

    float4 lightSpacePos = mul(u_LightVPs[cascade], float4(fragPos, 1.0));
    float3 proj = lightSpacePos.xyz / lightSpacePos.w;
    float2 uv   = float2(proj.x * 0.5 + 0.5, -proj.y * 0.5 + 0.5); // DX11 Y-flip
    float  curr = proj.z;

    if (curr > 1.0) return 1.0;

    float texelSize = 1.0 / u_ShadowMapSize;
    float shadow = 0.0;
    [unroll] for (int x = -1; x <= 1; ++x)
    [unroll] for (int y = -1; y <= 1; ++y)
        shadow += u_ShadowMapArray.SampleCmpLevelZero(u_ShadowSampler,
                  float3(uv + float2(x, y) * texelSize, (float)cascade), curr);
    return shadow / 9.0;
}

// =============================================================================
// PBR helpers (same as PBR_Mesh.hlsl)
// =============================================================================

static const float PI = 3.14159265359;

float D_GGX(float NdotH, float roughness)
{
    float a  = roughness * roughness;
    float a2 = a * a;
    float d  = NdotH * NdotH * (a2 - 1.0) + 1.0;
    return a2 / (PI * d * d);
}

float G_SchlickGGX(float NdotV, float roughness)
{
    float k = (roughness + 1.0) * (roughness + 1.0) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}

float G_Smith(float NdotV, float NdotL, float roughness)
{
    return G_SchlickGGX(NdotV, roughness) * G_SchlickGGX(NdotL, roughness);
}

float3 F_Schlick(float HdotV, float3 F0)
{
    return F0 + (1.0 - F0) * pow(1.0 - HdotV, 5.0);
}

float3 CookTorrance(float3 N, float3 V, float3 L, float3 albedo,
                    float metallic, float roughness, float3 lightColor, float intensity)
{
    float3 H = normalize(V + L);
    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.0);
    float NdotH = max(dot(N, H), 0.0);
    float HdotV = max(dot(H, V), 0.0);

    float3 F0 = lerp(float3(0.04, 0.04, 0.04), albedo, metallic);
    float3 F  = F_Schlick(HdotV, F0);
    float  D  = D_GGX(NdotH, max(roughness, 0.001));
    float  G  = G_Smith(NdotV, NdotL, roughness);

    float3 specular = (D * G * F) / max(4.0 * NdotV * NdotL, 0.001);
    float3 kd = (1.0 - F) * (1.0 - metallic);
    return (kd * albedo / PI + specular) * lightColor * intensity * NdotL;
}

// =============================================================================
// Vertex shader
// =============================================================================

#type vertex

struct VS_IN
{
    float3  a_Position    : a_Position;
    float3  a_Normal      : a_Normal;
    float2  a_TexCoord    : a_TexCoord;
    float3  a_Tangent     : a_Tangent;
    int4    a_BoneIDs     : a_BoneIDs;
    float4  a_BoneWeights : a_BoneWeights;
};

struct VS_OUT
{
    float4 Position  : SV_Position;
    float3 FragPos   : FRAGPOS;
    float3 Normal    : NORMAL;
    float2 TexCoord  : TEXCOORD;
    float3 ViewDir   : VIEWDIR;
    float  ViewDepth : DEPTH;
};

VS_OUT main(VS_IN IN)
{
    VS_OUT OUT;

    // Compute blended skinning matrix
    float4x4 skinMat = (float4x4)0;
    skinMat += IN.a_BoneWeights[0] * u_Bones[IN.a_BoneIDs[0]];
    skinMat += IN.a_BoneWeights[1] * u_Bones[IN.a_BoneIDs[1]];
    skinMat += IN.a_BoneWeights[2] * u_Bones[IN.a_BoneIDs[2]];
    skinMat += IN.a_BoneWeights[3] * u_Bones[IN.a_BoneIDs[3]];

    // If all weights zero (static mesh using this shader), fall through to identity
    float wsum = IN.a_BoneWeights[0] + IN.a_BoneWeights[1]
               + IN.a_BoneWeights[2] + IN.a_BoneWeights[3];
    if (wsum < 0.001)
        skinMat = float4x4(1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1);

    float4 skinnedPos    = mul(skinMat, float4(IN.a_Position, 1.0));
    float4 worldPos      = mul(u_Model, skinnedPos);

    float3 skinnedNormal = normalize(mul((float3x3)skinMat, IN.a_Normal));

    OUT.FragPos   = worldPos.xyz;
    OUT.Position  = mul(u_ViewProjectionMatrix, worldPos);
    OUT.Normal    = normalize(mul(u_NormalMatrix, skinnedNormal));
    OUT.TexCoord  = IN.a_TexCoord;
    OUT.ViewDir   = normalize(u_ViewPos - worldPos.xyz);

    float4 viewPos = mul(float4x4(1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1), worldPos); // just world
    OUT.ViewDepth = length(u_ViewPos - worldPos.xyz);
    return OUT;
}

// =============================================================================
// Pixel shader
// =============================================================================

#type pixel

struct PS_IN
{
    float4 Position  : SV_Position;
    float3 FragPos   : FRAGPOS;
    float3 Normal    : NORMAL;
    float2 TexCoord  : TEXCOORD;
    float3 ViewDir   : VIEWDIR;
    float  ViewDepth : DEPTH;
};

struct PS_OUT
{
    float4 Color    : SV_Target0;
    int    EntityID : SV_Target1;
};

PS_OUT main(PS_IN IN)
{
    PS_OUT OUT;

    float3 albedo = u_HasAlbedoMap
        ? u_AlbedoTexture.Sample(u_LinearSampler, IN.TexCoord).rgb * u_AlbedoColor.rgb
        : u_AlbedoColor.rgb;

    float3 N = normalize(IN.Normal);
    if (u_HasNormalMap)
    {
        float3 nmap = u_NormalTexture.Sample(u_LinearSampler, IN.TexCoord).rgb * 2.0 - 1.0;
        N = normalize(nmap); // simplified (no TBN in this version)
    }

    float3 V = normalize(IN.ViewDir);

    float3 color = float3(0, 0, 0);

    float shadow = u_HasShadowMap ? CalcCSMShadow(IN.FragPos, IN.ViewDepth) : 1.0;

    float3 F0 = lerp(float3(0.04, 0.04, 0.04), albedo, u_Metallic);

    for (int i = 0; i < u_NumDirLights; ++i)
    {
        float3 L = normalize(-u_DirLights[i].Direction);
        color += CookTorrance(N, V, L, albedo, u_Metallic, u_Roughness,
                              u_DirLights[i].Diffuse, u_DirLights[i].Intensity) * shadow;
    }

    for (int j = 0; j < u_NumPointLights; ++j)
    {
        float3 toL = u_PointLights[j].Position - IN.FragPos;
        float dist = length(toL);
        if (dist < u_PointLights[j].Range)
        {
            float3 L  = normalize(toL);
            float att = 1.0 / dot(u_PointLights[j].Att, float3(1, dist, dist * dist));
            color += CookTorrance(N, V, L, albedo, u_Metallic, u_Roughness,
                                  u_PointLights[j].Diffuse, u_PointLights[j].Intensity) * att;
        }
    }

    // ─── IBL / fallback ambient ───────────────────────────────────────────────
    if (u_IBLEnabled)
    {
        float3 kS = F0 + (1.0 - F0) * pow(1.0 - max(dot(N, V), 0.0), 5.0);
        float3 kD = (1.0 - kS) * (1.0 - u_Metallic);

        float3 irradiance  = u_IrradianceMap.Sample(u_LinearSampler, N).rgb;
        float3 diffuseIBL  = kD * irradiance * albedo;

        float3 R = reflect(-V, N);
        float  mipLevel = u_Roughness * float(u_IBLMipLevels - 1);
        float3 prefiltered = u_PrefilteredEnv.SampleLevel(u_LinearSampler, R, mipLevel).rgb;
        float2 brdf = u_BRDFLUT.Sample(u_LinearSampler,
                                        float2(max(dot(N, V), 0.0), u_Roughness)).rg;
        float3 specularIBL = prefiltered * (F0 * brdf.x + brdf.y);

        color += (diffuseIBL + specularIBL) * u_AO * u_IBLStrength;
    }
    else
    {
        for (int k = 0; k < u_NumDirLights; ++k)
            color += u_DirLights[k].Ambient * albedo * u_AO;
        for (int m = 0; m < u_NumPointLights; ++m)
        {
            float3 toL2 = u_PointLights[m].Position - IN.FragPos;
            float d2    = length(toL2);
            if (d2 < u_PointLights[m].Range)
            {
                float att2 = 1.0 / dot(u_PointLights[m].Att, float3(1, d2, d2 * d2));
                color += u_PointLights[m].Ambient * albedo * u_AO * att2;
            }
        }
    }

    // Emissive
    float3 emissive = u_HasEmissiveMap ? float3(0, 0, 0) : u_EmissiveColor * u_EmissiveStrength;
    color += emissive;

    // Fog
    if (u_FogEnabled)
    {
        float dist3D  = length(u_ViewPos - IN.FragPos);
        float heightF = exp(-max(0.0, (u_ViewPos.y - IN.FragPos.y) - u_FogHeightStart) * u_FogHeightDensity);
        float fogAmt  = 1.0 - exp(-u_FogDensity * dist3D * heightF);
        float3 fc     = lerp(u_FogColor, u_AerialColor, saturate(fogAmt * u_AerialStrength));
        color = lerp(color, fc, saturate(fogAmt));
    }

    // ACES tone-map
    float3 x = color;
    color = (x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14);
    color = saturate(color);

    OUT.Color    = float4(color, 1.0);
    OUT.EntityID = u_EntityID;
    return OUT;
}
