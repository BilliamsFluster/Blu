// Foliage_Instanced.hlsl
// GPU-instanced vegetation shader.
// Up to 256 instances per draw call; transforms stored in InstanceData cbuffer.
// Wind: simple sine-wave vertex offset along WindDir, scaled by vertex height (y).

#pragma pack_matrix(column_major)

// =============================================================================
// Constant buffers  (registers mirror PBR_Mesh.hlsl for shared light data)
// =============================================================================

cbuffer PerFrame : register(b0)
{
    float4x4 u_ViewProjectionMatrix;
    float3   u_ViewPos;
    int      u_HasShadowMap;
    float3   u_FogColor;          float u_FogDensity;
    float    u_FogHeightStart;    float u_FogHeightDensity;
    int      u_FogEnabled;        float u_AerialStrength;
    float3   u_AerialColor;       float _fogPad;
};

cbuffer InstanceData : register(b1)
{
    float4x4 u_Transforms[256];   // 256 * 64 = 16 384 bytes, fits in cbuffer
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

cbuffer LightData : register(b3)
{
    DirectionalLight u_DirLights[4];
    PointLight       u_PointLights[8];
    int  u_NumDirLights;
    int  u_NumPointLights;
    int  u_NumSpotLights;
    float _padL;
};

cbuffer WindData : register(b5)
{
    float3 u_WindDirection; float u_WindStrength;
    float  u_WindFrequency; float u_Time;
    float2 _windPad;
};

// =============================================================================
// Textures
// =============================================================================

Texture2D    u_AlbedoTexture : register(t0);
SamplerState u_LinearSampler : register(s0);

// =============================================================================
// Vertex shader
// =============================================================================

#type vertex

struct VS_IN
{
    float3 a_Position : a_Position;
    float3 a_Normal   : a_Normal;
    float2 a_TexCoord : a_TexCoord;
    uint   InstanceID : SV_InstanceID;
};

struct VS_OUT
{
    float4 Position : SV_Position;
    float3 FragPos  : FRAGPOS;
    float3 Normal   : NORMAL;
    float2 TexCoord : TEXCOORD;
};

VS_OUT main(VS_IN IN)
{
    VS_OUT OUT;

    float4x4 model = u_Transforms[IN.InstanceID];

    // Wind: offset the vertex horizontally based on height (IN.a_Position.y)
    // and a sine wave driven by world-X position + time.
    float worldX    = mul(model, float4(IN.a_Position, 1.0)).x;
    float windPhase = worldX * 0.5 + u_Time * u_WindFrequency;
    float windOff   = sin(windPhase) * u_WindStrength * max(0.0, IN.a_Position.y);
    float3 posWind  = IN.a_Position + u_WindDirection * windOff;

    float4 worldPos = mul(model, float4(posWind, 1.0));
    OUT.FragPos     = worldPos.xyz;
    OUT.Position    = mul(u_ViewProjectionMatrix, worldPos);

    float3x3 normalMat = (float3x3)transpose(model); // approximate; works for uniform scale
    OUT.Normal   = normalize(mul(normalMat, IN.a_Normal));
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

float3 CalcDirLight(DirectionalLight L, float3 N, float3 V, float3 albedo)
{
    float3 lightDir = normalize(-L.Direction);
    float  diff     = max(dot(N, lightDir), 0.0);
    float3 R        = reflect(-lightDir, N);
    float  spec     = pow(max(dot(V, R), 0.0), 32.0);

    float3 ambient  = L.Ambient  * albedo * L.Intensity;
    float3 diffuse  = L.Diffuse  * diff   * albedo * L.Intensity;
    float3 specular = L.Specular * spec   * 0.1    * L.Intensity;
    return ambient + diffuse + specular;
}

float4 main(PS_IN IN) : SV_Target0
{
    float4 texColor = u_HasAlbedoMap ? u_AlbedoTexture.Sample(u_LinearSampler, IN.TexCoord)
                                     : u_AlbedoColor;

    // Alpha cutout for leaf cards
    clip(texColor.a - u_AlphaCutoff);

    float3 albedo = texColor.rgb * u_AlbedoColor.rgb;
    float3 N = normalize(IN.Normal);
    float3 V = normalize(u_ViewPos - IN.FragPos);

    float3 color = float3(0, 0, 0);
    for (int i = 0; i < u_NumDirLights; ++i)
        color += CalcDirLight(u_DirLights[i], N, V, albedo);

    // Point lights (simplified, no specular for perf)
    for (int j = 0; j < u_NumPointLights; ++j)
    {
        PointLight PL = u_PointLights[j];
        float3 toLight = PL.Position - IN.FragPos;
        float  dist    = length(toLight);
        if (dist < PL.Range)
        {
            float3 ld   = normalize(toLight);
            float  att  = 1.0 / dot(PL.Att, float3(1, dist, dist * dist));
            float  diff = max(dot(N, ld), 0.0);
            color += PL.Diffuse * diff * albedo * PL.Intensity * att;
        }
    }

    // Ambient minimum
    if (u_NumDirLights == 0 && u_NumPointLights == 0)
        color = albedo * 0.3;

    // Fog
    if (u_FogEnabled)
    {
        float dist3D  = length(u_ViewPos - IN.FragPos);
        float heightF = exp(-max(0.0, (u_ViewPos.y - IN.FragPos.y) - u_FogHeightStart) * u_FogHeightDensity);
        float fogAmt  = 1.0 - exp(-u_FogDensity * dist3D * heightF);
        float3 fc     = lerp(u_FogColor, u_AerialColor, saturate(fogAmt * u_AerialStrength));
        color = lerp(color, fc, saturate(fogAmt));
    }

    return float4(color, texColor.a);
}
