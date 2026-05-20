// PBR_Mesh.hlsl
// Cook-Torrance BRDF with GGX microfacet distribution.
// Expected texture packing:
//   t0: Albedo              (sRGB)
//   t1: Normal              (linear)
//   t2: MetallicRoughness   (B=Metallic, G=Roughness)
//   t3: AO                  (linear)
//   t4: Emissive            (sRGB)
//   t5: Shadow map

// =============================================================================
// Constant buffers
// =============================================================================

cbuffer PerFrame : register(b0)
{
    float4x4 u_ViewProjectionMatrix;   // 64 bytes
    float3   u_ViewPos;                // 12 bytes
    int      u_HasShadowMap;           // 4 bytes → 80 total

    // Exponential height fog
    float3   u_FogColor;               // 12 bytes
    float    u_FogDensity;             // 4 bytes → 96 total
    float    u_FogHeightStart;         // 4 bytes
    float    u_FogHeightDensity;       // 4 bytes
    int      u_FogEnabled;             // 4 bytes
    float    u_AerialStrength;         // 4 bytes → 112 total

    // Aerial perspective: fog colour blends toward sky horizon at distance
    float3   u_AerialColor;            // 12 bytes
    float    _fogPad;                  // 4 bytes → 128 total
};

cbuffer PerObject : register(b1)
{
    float4x4 u_Model;
    float3x3 u_NormalMatrix;
    int      u_EntityID;  float3 _padPerObj;
};

cbuffer MaterialData : register(b2)
{
    float4  u_AlbedoColor;                                              // 16 bytes
    float   u_Metallic;       float  u_Roughness;
    float   u_AO;             float  u_EmissiveStrength;                // 16 bytes
    float3  u_EmissiveColor;  float  u_AlphaCutoff;                     // 16 bytes
    int     u_ShadingModel;   int    u_HasAlbedoMap;
    int     u_HasNormalMap;   int    u_HasMetallicRoughnessMap;         // 16 bytes
    int     u_HasAOMap;       int    u_HasEmissiveMap;
    int2    _matPad;                                                     // 16 bytes
};

// ─── Lights ───────────────────────────────────────────────────────────────────

struct DirectionalLight
{
    float3 Direction;  float _p0;
    float3 Ambient;    float Intensity;
    float3 Diffuse;    float _p1;
    float3 Specular;   float _p2;
};

struct PointLight
{
    float3 Position;   float Range;
    float3 Ambient;    float Intensity;
    float3 Diffuse;    float _p0;
    float3 Specular;   float _p1;
    float3 Att;        float _p2;
};

struct SpotLight
{
    float3 Position;     float Range;
    float3 Direction;    float Intensity;
    float3 Ambient;      float _p0;
    float3 Diffuse;      float _p1;
    float3 Specular;     float _p2;
    float3 Att;          float InnerCutoff;
    float  OuterCutoff;  float3 _p3;
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
    float4x4 u_LightVPs[3];    // 192 bytes — one per cascade
    float3   u_CascadeSplits;  // world-space distance thresholds
    float    u_ShadowMapSize;  // texel size denominator
};

cbuffer IBLData : register(b6)
{
    int    u_IBLEnabled;
    float  u_IBLStrength;
    int    u_IBLMipLevels; // kPrefilterMips from IBLSystem
    float  _iblPad;
};

Texture2DArray           u_ShadowMapArray : register(t5);
SamplerComparisonState   u_ShadowSampler  : register(s1);

// ─── Material textures ────────────────────────────────────────────────────────

Texture2D    u_AlbedoTexture             : register(t0);
Texture2D    u_NormalTexture             : register(t1);
Texture2D    u_MetallicRoughnessTexture  : register(t2);
Texture2D    u_AOTexture                 : register(t3);
Texture2D    u_EmissiveTexture           : register(t4);

// ─── IBL textures (bound by IBLSystem::BindIBL) ───────────────────────────────
TextureCube  u_IrradianceMap  : register(t6);  // diffuse  — 32x32, 1 mip
TextureCube  u_PrefilteredEnv : register(t7);  // specular — 128x128, 5 mips
Texture2D    u_BRDFLUT        : register(t8);  // split-sum LUT: R=scale G=bias

SamplerState u_LinearSampler             : register(s0);

// =============================================================================
// PBR helper functions
// =============================================================================

float D_GGX(float NdotH, float roughness)
{
    float a  = roughness * roughness;
    float a2 = a * a;
    float d  = NdotH * NdotH * (a2 - 1.0) + 1.0;
    return a2 / (3.14159 * d * d);
}

float3 F_Schlick(float cosTheta, float3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
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

float3 Fd_Lambert(float3 albedo)
{
    return albedo / 3.14159;
}

float3 SampleNormalMap(float2 uv, float3 N, float3 T)
{
    float3 tangentNormal = u_NormalTexture.Sample(u_LinearSampler, uv).xyz * 2.0 - 1.0;
    float3 B   = cross(N, T);
    float3x3 TBN = float3x3(T, B, N);
    return normalize(mul(tangentNormal, TBN));
}

float CalcCSMShadowFactor(float3 fragPos)
{
    // Select cascade by distance from camera
    float dist = distance(fragPos, u_ViewPos);
    int   cIdx = 2;
    if      (dist < u_CascadeSplits.x) cIdx = 0;
    else if (dist < u_CascadeSplits.y) cIdx = 1;

    // Transform to light clip space (u_LightVPs use RH_ZO: Z output is in [0,1])
    float4 posLS = mul(u_LightVPs[cIdx], float4(fragPos, 1.0));
    float3 proj  = posLS.xyz / posLS.w;

    // UV: NDC XY [-1,1] → [0,1]; Y flipped for DX11 texture space
    float2 uv = float2(proj.x * 0.5 + 0.5, -proj.y * 0.5 + 0.5);

    // Depth is already in [0,1] from RH_ZO
    float currentDepth = proj.z;

    if (currentDepth < 0.0 || currentDepth > 1.0) return 1.0;
    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) return 1.0;

    float texelSize = 1.0 / u_ShadowMapSize;
    float shadow = 0.0;
    [unroll]
    for (int x = -1; x <= 1; ++x)
        [unroll]
        for (int y = -1; y <= 1; ++y)
            shadow += u_ShadowMapArray.SampleCmpLevelZero(u_ShadowSampler,
                          float3(uv + float2(x, y) * texelSize, (float)cIdx),
                          currentDepth);
    return shadow / 9.0;
}

float3 CalcDirLight(DirectionalLight L, float3 N, float3 V, float3 F0,
                    float3 albedo, float metallic, float roughness, float shadowFactor)
{
    float3 lightDir = normalize(-L.Direction);
    float3 H = normalize(V + lightDir);

    float NdotV = max(dot(N, V),        0.001);
    float NdotL = max(dot(N, lightDir), 0.001);
    float NdotH = max(dot(N, H),        0.0);
    float HdotV = max(dot(H, V),        0.0);

    float  D = D_GGX(NdotH, roughness);
    float3 F = F_Schlick(HdotV, F0);
    float  G = G_Smith(NdotV, NdotL, roughness);

    float3 specular = (D * F * G) / max(4.0 * NdotV * NdotL, 0.001);
    float3 diffuse  = Fd_Lambert(albedo) * (1.0 - F);

    return (diffuse + specular) * L.Diffuse * L.Intensity * NdotL * shadowFactor;
}

float3 CalcPointLight(PointLight L, float3 N, float3 V, float3 fragPos, float3 F0,
                      float3 albedo, float metallic, float roughness)
{
    float3 toLight  = L.Position - fragPos;
    float  d        = length(toLight);
    if (d > L.Range) return 0.0;

    float3 lightDir = toLight / d;
    float3 H = normalize(V + lightDir);

    float NdotV = max(dot(N, V),        0.001);
    float NdotL = max(dot(N, lightDir), 0.001);
    float NdotH = max(dot(N, H),        0.0);
    float HdotV = max(dot(H, V),        0.0);

    float  D = D_GGX(NdotH, roughness);
    float3 F = F_Schlick(HdotV, F0);
    float  G = G_Smith(NdotV, NdotL, roughness);

    float3 specular = (D * F * G) / max(4.0 * NdotV * NdotL, 0.001);
    float3 diffuse  = Fd_Lambert(albedo) * (1.0 - F);

    float att = 1.0 / dot(L.Att, float3(1.0, d, d * d));
    return (diffuse + specular) * L.Diffuse * L.Intensity * att * NdotL;
}

float3 CalcSpotLight(SpotLight L, float3 N, float3 V, float3 fragPos, float3 F0,
                     float3 albedo, float metallic, float roughness)
{
    float3 toLight = L.Position - fragPos;
    float  d       = length(toLight);
    if (d > L.Range) return 0.0;

    float3 lightDir = toLight / d;
    float3 H = normalize(V + lightDir);

    float NdotV = max(dot(N, V),        0.001);
    float NdotL = max(dot(N, lightDir), 0.001);
    float NdotH = max(dot(N, H),        0.0);
    float HdotV = max(dot(H, V),        0.0);

    float  D = D_GGX(NdotH, roughness);
    float3 F = F_Schlick(HdotV, F0);
    float  G = G_Smith(NdotV, NdotL, roughness);

    float3 specular = (D * F * G) / max(4.0 * NdotV * NdotL, 0.001);
    float3 diffuse  = Fd_Lambert(albedo) * (1.0 - F);

    float att        = 1.0 / dot(L.Att, float3(1.0, d, d * d));
    float theta      = dot(lightDir, normalize(-L.Direction));
    float epsilon    = L.InnerCutoff - L.OuterCutoff;
    float spotFactor = saturate((theta - L.OuterCutoff) / epsilon);

    return (diffuse + specular) * L.Diffuse * L.Intensity * att * spotFactor * NdotL;
}

// =============================================================================
// Vertex shader
// =============================================================================

#type vertex

struct VS_IN
{
    float3 a_Position : a_Position;
    float3 a_Normal   : a_Normal;
    float2 a_TexCoord : a_TexCoord;
    float3 a_Tangent  : a_Tangent;
};

struct VS_OUT
{
    float4 Position : SV_Position;
    float3 FragPos  : FRAGPOS;
    float3 Normal   : NORMAL;
    float2 TexCoord : TEXCOORD;
    float3 Tangent  : TANGENT;
};

VS_OUT main(VS_IN IN)
{
    VS_OUT OUT;
    float4 worldPos = mul(u_Model, float4(IN.a_Position, 1.0));
    OUT.FragPos     = worldPos.xyz;
    OUT.Position    = mul(u_ViewProjectionMatrix, worldPos);
    OUT.Normal      = normalize(mul(u_NormalMatrix, IN.a_Normal));
    OUT.Tangent     = normalize(mul((float3x3)u_Model, IN.a_Tangent));
    OUT.TexCoord    = IN.a_TexCoord;
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
    float3 Tangent  : TANGENT;
};

struct PS_OUT
{
    float4 Color    : SV_Target0;
    int    EntityID : SV_Target1;
};

PS_OUT main(PS_IN IN)
{
    PS_OUT OUT;
    float2 uv = IN.TexCoord;

    // Sample albedo — carry alpha through for Transparent/Masked modes
    float4 albedoSample = u_HasAlbedoMap
        ? u_AlbedoTexture.Sample(u_LinearSampler, uv)
        : float4(1.0, 1.0, 1.0, 1.0);
    float3 albedo = u_AlbedoColor.rgb * albedoSample.rgb;
    float  alpha  = u_AlbedoColor.a   * albedoSample.a;

    // Alpha cutoff for Masked mode (u_AlphaCutoff > 0 means Masked is active)
    if (u_AlphaCutoff > 0.0)
        clip(alpha - u_AlphaCutoff);

    float metallic  = u_Metallic;
    float roughness = u_Roughness;
    if (u_HasMetallicRoughnessMap)
    {
        float3 mr  = u_MetallicRoughnessTexture.Sample(u_LinearSampler, uv).rgb;
        metallic   = mr.b;
        roughness  = mr.g;
    }

    float ao = u_AO;
    if (u_HasAOMap)
        ao = u_AOTexture.Sample(u_LinearSampler, uv).r;

    float3 N = normalize(IN.Normal);
    if (u_HasNormalMap)
    {
        float3 T = normalize(IN.Tangent);
        N = SampleNormalMap(uv, N, T);
    }

    // Unlit shading path — skip all lighting, output albedo + emissive only
    if (u_ShadingModel == 1)
    {
        float3 emissive = u_EmissiveColor * u_EmissiveStrength;
        if (u_HasEmissiveMap)
            emissive *= u_EmissiveTexture.Sample(u_LinearSampler, uv).rgb;
        OUT.Color    = float4(albedo + emissive, alpha);
        OUT.EntityID = u_EntityID;
        return OUT;
    }

    // PBR lighting path
    float3 F0 = lerp(float3(0.04, 0.04, 0.04), albedo, metallic);
    float3 V  = normalize(u_ViewPos - IN.FragPos);

    float shadowFactor = u_HasShadowMap ? CalcCSMShadowFactor(IN.FragPos) : 1.0;

    float3 result = float3(0.0, 0.0, 0.0);

    // ─── Direct lighting ─────────────────────────────────────────────────────────
    for (int i = 0; i < u_NumDirLights;   ++i)
        result += CalcDirLight  (u_DirLights[i],   N, V, F0, albedo, metallic, roughness, shadowFactor);
    for (int i = 0; i < u_NumPointLights; ++i)
        result += CalcPointLight(u_PointLights[i], N, V, IN.FragPos, F0, albedo, metallic, roughness);
    for (int i = 0; i < u_NumSpotLights;  ++i)
        result += CalcSpotLight (u_SpotLights[i],  N, V, IN.FragPos, F0, albedo, metallic, roughness);

    // ─── IBL ambient (image-based lighting) ─────────────────────────────────────
    if (u_IBLEnabled)
    {
        float3 kS = F_Schlick(max(dot(N, V), 0.0), F0);
        float3 kD = (1.0 - kS) * (1.0 - metallic);

        // Diffuse IBL: irradiance convolution of hemisphere around N
        float3 irradiance = u_IrradianceMap.Sample(u_LinearSampler, N).rgb;
        float3 diffuseIBL = kD * irradiance * albedo;

        // Specular IBL: split-sum (prefiltered env + BRDF LUT)
        float3 R = reflect(-V, N);
        float  mipLevel = roughness * float(u_IBLMipLevels - 1);
        float3 prefilteredColor = u_PrefilteredEnv.SampleLevel(u_LinearSampler, R, mipLevel).rgb;
        float2 brdf = u_BRDFLUT.Sample(u_LinearSampler,
                                        float2(max(dot(N, V), 0.0), roughness)).rg;
        // BRDF LUT is stored as UNORM8 → remap [0,255]→[0,1] happens automatically.
        // Scale and bias are in [0,1]; reconstruct specular contribution:
        float3 specularIBL = prefilteredColor * (F0 * brdf.x + brdf.y);

        result += (diffuseIBL + specularIBL) * ao * u_IBLStrength;
    }
    else
    {
        // Fallback: simple directional ambient
        float3 ambient = (u_NumDirLights > 0)
            ? u_DirLights[0].Ambient * u_DirLights[0].Intensity * albedo * ao
            : 0.03 * albedo * ao;
        result += ambient;
    }

    float3 emissive = u_EmissiveColor * u_EmissiveStrength;
    if (u_HasEmissiveMap)
        emissive *= u_EmissiveTexture.Sample(u_LinearSampler, uv).rgb;
    result += emissive;

    // Exponential height fog + aerial perspective
    if (u_FogEnabled)
    {
        float  dist      = length(u_ViewPos - IN.FragPos);
        float  fogFactor = exp(-u_FogDensity * dist);

        if (u_FogHeightDensity > 0.0f)
        {
            float heightAbove = max(0.0f, IN.FragPos.y - u_FogHeightStart);
            fogFactor *= exp(-heightAbove * u_FogHeightDensity);
        }
        fogFactor = saturate(fogFactor);

        // Aerial perspective: horizon fog blends toward sky colour, eliminating
        // the flat white band that appears when fog colour != sky colour.
        float3 viewDir      = normalize(IN.FragPos - u_ViewPos);
        float  horizonBlend = saturate(1.0f - abs(viewDir.y));
        float3 finalFog     = lerp(u_FogColor, u_AerialColor,
                                   u_AerialStrength * horizonBlend);

        result = lerp(finalFog, result, fogFactor);
    }

    OUT.Color    = float4(result, alpha);
    OUT.EntityID = u_EntityID;
    return OUT;
}
