// Renderer2D_Quad.hlsl
// Batched quad rendering with optional Phong lighting.

#pragma pack_matrix(column_major)

// -----------------------------------------------------------------------
// Constant buffers
// -----------------------------------------------------------------------

cbuffer PerFrame : register(b0)
{
    float4x4 u_ViewProjectionMatrix;
    float3   u_ViewPos;
    float    _pad0;
};

struct Light
{
    float3 position;
    float  _p0;
    float3 ambient;
    float  _p1;
    float3 diffuse;
    float  _p2;
    float3 specular;
    float  _p3;
};

cbuffer LightData : register(b1)
{
    Light u_Lights[8];
    int   u_NumLights;
    float3 _padL;
};

// -----------------------------------------------------------------------
// Vertex shader
// -----------------------------------------------------------------------

#type vertex

Texture2D    u_Textures[16] : register(t0);
SamplerState u_Sampler      : register(s0);

struct VS_IN
{
    float3 a_Position     : a_Position;
    float4 a_Color        : a_Color;
    float2 a_TexCoord     : a_TexCoord;
    float  a_TexIndex     : a_TexIndex;
    float  a_TilingFactor : a_TilingFactor;
    int    a_EntityID     : a_EntityID;
    float3 a_Normal       : a_Normal;
};

struct VS_OUT
{
    float4 Position     : SV_Position;
    float4 Color        : COLOR;
    float2 TexCoord     : TEXCOORD;
    float  TexIndex     : TEXINDEX;
    float  TilingFactor : TILINGFACTOR;
    int    EntityID     : ENTITYID;
    float3 FragPos      : FRAGPOS;
    float3 Normal       : NORMAL;
};

VS_OUT main(VS_IN IN)
{
    VS_OUT OUT;
    OUT.Position     = mul(u_ViewProjectionMatrix, float4(IN.a_Position, 1.0));
    OUT.Color        = IN.a_Color;
    OUT.TexCoord     = IN.a_TexCoord;
    OUT.TexIndex     = IN.a_TexIndex;
    OUT.TilingFactor = IN.a_TilingFactor;
    OUT.EntityID     = IN.a_EntityID;
    OUT.FragPos      = IN.a_Position;
    OUT.Normal       = IN.a_Normal;
    return OUT;
}

// -----------------------------------------------------------------------
// Pixel shader
// -----------------------------------------------------------------------

#type pixel

Texture2D    u_Textures[16] : register(t0);
SamplerState u_Sampler      : register(s0);

struct PS_IN
{
    float4 Position     : SV_Position;
    float4 Color        : COLOR;
    float2 TexCoord     : TEXCOORD;
    float  TexIndex     : TEXINDEX;
    float  TilingFactor : TILINGFACTOR;
    int    EntityID     : ENTITYID;
    float3 FragPos      : FRAGPOS;
    float3 Normal       : NORMAL;
};

struct PS_OUT
{
    float4 Color    : SV_Target0;
    int    EntityID : SV_Target1;
};

float3 CalcPointLight(Light light, float3 normal, float3 fragPos, float3 viewDir, float3 baseColor)
{
    float3 lightDir   = normalize(light.position - fragPos);
    float3 reflectDir = reflect(-lightDir, normal);
    float  diff       = max(dot(normal, lightDir), 0.0);
    float  spec       = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);
    return (light.ambient + diff * light.diffuse + spec * light.specular) * baseColor;
}

PS_OUT main(PS_IN IN)
{
    PS_OUT OUT;

    float4 texColor = u_Textures[0].Sample(u_Sampler, IN.TexCoord * IN.TilingFactor);
    float4 baseColor = texColor * IN.Color;

    if (u_NumLights > 0)
    {
        float3 normal  = normalize(IN.Normal);
        float3 viewDir = normalize(u_ViewPos - IN.FragPos);
        float3 lit     = float3(0, 0, 0);
        for (int i = 0; i < u_NumLights; i++)
            lit += CalcPointLight(u_Lights[i], normal, IN.FragPos, viewDir, baseColor.rgb);
        OUT.Color = float4(lit, baseColor.a);
    }
    else
    {
        OUT.Color = baseColor;
    }

    OUT.EntityID = IN.EntityID;
    return OUT;
}
