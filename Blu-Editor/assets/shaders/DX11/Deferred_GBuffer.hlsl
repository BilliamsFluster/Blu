cbuffer PerFrame : register(b0)
{
    float4x4 u_ViewProjectionMatrix;
};

cbuffer PerObject : register(b1)
{
    float4x4 u_Model;
    float3x3 u_NormalMatrix;
    int u_EntityID; float3 _padPerObject;
};

cbuffer MaterialData : register(b2)
{
    float4 u_AlbedoColor;
    float u_Metallic; float u_Roughness;
    float u_AO; float u_EmissiveStrength;
    float3 u_EmissiveColor; float u_AlphaCutoff;
    int u_ShadingModel; int u_HasAlbedoMap;
    int u_HasNormalMap; int u_HasMetallicRoughnessMap;
    int u_HasAOMap; int u_HasEmissiveMap;
    int2 _matPad;
};

Texture2D u_AlbedoTexture : register(t0);
Texture2D u_NormalTexture : register(t1);
Texture2D u_MetallicRoughnessTexture : register(t2);
Texture2D u_AOTexture : register(t3);
Texture2D u_EmissiveTexture : register(t4);
SamplerState u_LinearSampler : register(s0);

float3 SampleNormalMap(float2 uv, float3 normal, float3 tangent)
{
    float3 tangentNormal = u_NormalTexture.Sample(u_LinearSampler, uv).xyz * 2.0 - 1.0;
    float3 bitangent = cross(normal, tangent);
    return normalize(mul(tangentNormal, float3x3(tangent, bitangent, normal)));
}

#type vertex

struct VS_IN
{
    float3 a_Position : a_Position;
    float3 a_Normal : a_Normal;
    float2 a_TexCoord : a_TexCoord;
    float3 a_Tangent : a_Tangent;
};

struct VS_OUT
{
    float4 Position : SV_Position;
    float3 FragPos : FRAGPOS;
    float3 Normal : NORMAL;
    float2 TexCoord : TEXCOORD;
    float3 Tangent : TANGENT;
};

VS_OUT main(VS_IN input)
{
    VS_OUT output;
    float4 worldPosition = mul(u_Model, float4(input.a_Position, 1.0));
    output.Position = mul(u_ViewProjectionMatrix, worldPosition);
    output.FragPos = worldPosition.xyz;
    output.Normal = normalize(mul(u_NormalMatrix, input.a_Normal));
    output.Tangent = normalize(mul((float3x3)u_Model, input.a_Tangent));
    output.TexCoord = input.a_TexCoord;
    return output;
}

#type pixel

struct PS_IN
{
    float4 Position : SV_Position;
    float3 FragPos : FRAGPOS;
    float3 Normal : NORMAL;
    float2 TexCoord : TEXCOORD;
    float3 Tangent : TANGENT;
};

struct PS_OUT
{
    float4 Position : SV_Target0;
    float4 Normal : SV_Target1;
    float4 AlbedoAO : SV_Target2;
    float4 Material : SV_Target3;
    float4 Emissive : SV_Target4;
    int EntityID : SV_Target5;
};

PS_OUT main(PS_IN input)
{
    PS_OUT output;
    float4 albedoSample = u_HasAlbedoMap
        ? u_AlbedoTexture.Sample(u_LinearSampler, input.TexCoord)
        : float4(1.0, 1.0, 1.0, 1.0);
    float4 albedo = u_AlbedoColor * albedoSample;
    if (u_AlphaCutoff > 0.0)
        clip(albedo.a - u_AlphaCutoff);

    float metallic = u_Metallic;
    float roughness = u_Roughness;
    if (u_HasMetallicRoughnessMap)
    {
        float3 metallicRoughness = u_MetallicRoughnessTexture.Sample(u_LinearSampler, input.TexCoord).rgb;
        metallic = metallicRoughness.b;
        roughness = metallicRoughness.g;
    }

    float ao = u_HasAOMap
        ? u_AOTexture.Sample(u_LinearSampler, input.TexCoord).r
        : u_AO;
    float3 normal = normalize(input.Normal);
    if (u_HasNormalMap)
        normal = SampleNormalMap(input.TexCoord, normal, normalize(input.Tangent));

    float3 emissive = u_EmissiveColor * u_EmissiveStrength;
    if (u_HasEmissiveMap)
        emissive *= u_EmissiveTexture.Sample(u_LinearSampler, input.TexCoord).rgb;

    output.Position = float4(input.FragPos, 1.0);
    output.Normal = float4(normal, 1.0);
    output.AlbedoAO = float4(albedo.rgb, ao);
    output.Material = float4(metallic, roughness, (float)u_ShadingModel, albedo.a);
    output.Emissive = float4(emissive, 1.0);
    output.EntityID = u_EntityID;
    return output;
}
