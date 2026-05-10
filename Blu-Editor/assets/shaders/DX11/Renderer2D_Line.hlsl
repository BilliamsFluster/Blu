// Renderer2D_Line.hlsl

#pragma pack_matrix(column_major)

cbuffer PerFrame : register(b0)
{
    float4x4 u_ViewProjectionMatrix;
    float3   u_ViewPos;
    float    _pad0;
};

// -----------------------------------------------------------------------
// Vertex shader
// -----------------------------------------------------------------------

#type vertex

struct VS_IN
{
    float4 a_Color     : a_Color;
    float3 a_Position  : a_Position;
    float  a_Thickness : a_Thickness;
    int    a_EntityID  : a_EntityID;
};

struct VS_OUT
{
    float4 Position  : SV_Position;
    float4 Color     : COLOR;
    float3 FragPos   : FRAGPOS;
    float  Thickness : THICKNESS;
    int    EntityID  : ENTITYID;
};

VS_OUT main(VS_IN IN)
{
    VS_OUT OUT;
    OUT.Position  = mul(u_ViewProjectionMatrix, float4(IN.a_Position, 1.0));
    OUT.Color     = IN.a_Color;
    OUT.FragPos   = IN.a_Position;
    OUT.Thickness = IN.a_Thickness;
    OUT.EntityID  = IN.a_EntityID;
    return OUT;
}

// -----------------------------------------------------------------------
// Pixel shader
// -----------------------------------------------------------------------

#type pixel

struct PS_IN
{
    float4 Position  : SV_Position;
    float4 Color     : COLOR;
    float3 FragPos   : FRAGPOS;
    float  Thickness : THICKNESS;
    int    EntityID  : ENTITYID;
};

struct PS_OUT
{
    float4 Color    : SV_Target0;
    int    EntityID : SV_Target1;
};

PS_OUT main(PS_IN IN)
{
    PS_OUT OUT;
    OUT.Color    = IN.Color;
    OUT.EntityID = IN.EntityID;
    return OUT;
}
