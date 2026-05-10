// Renderer2D_Circle.hlsl

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
    float4 a_Color         : a_Color;
    float3 a_WorldPosition : a_WorldPosition;
    float3 a_LocalPosition : a_LocalPosition;
    float  a_Fade          : a_Fade;
    float  a_Thickness     : a_Thickness;
    int    a_EntityID      : a_EntityID;
};

struct VS_OUT
{
    float4 Position      : SV_Position;
    float3 LocalPosition : LOCALPOS;
    float  Fade          : FADE;
    float4 Color         : COLOR;
    float  Thickness     : THICKNESS;
    int    EntityID      : ENTITYID;
};

VS_OUT main(VS_IN IN)
{
    VS_OUT OUT;
    OUT.Position      = mul(u_ViewProjectionMatrix, float4(IN.a_WorldPosition, 1.0));
    OUT.Color         = IN.a_Color;
    OUT.Fade          = IN.a_Fade;
    OUT.Thickness     = IN.a_Thickness;
    OUT.LocalPosition = IN.a_LocalPosition;
    OUT.EntityID      = IN.a_EntityID;
    return OUT;
}

// -----------------------------------------------------------------------
// Pixel shader
// -----------------------------------------------------------------------

#type pixel

struct PS_IN
{
    float4 Position      : SV_Position;
    float3 LocalPosition : LOCALPOS;
    float  Fade          : FADE;
    float4 Color         : COLOR;
    float  Thickness     : THICKNESS;
    int    EntityID      : ENTITYID;
};

struct PS_OUT
{
    float4 Color    : SV_Target0;
    int    EntityID : SV_Target1;
};

PS_OUT main(PS_IN IN)
{
    PS_OUT OUT;

    float dist        = 1.0 - length(IN.LocalPosition);
    float circleAlpha = smoothstep(0.0, IN.Fade, dist);
    circleAlpha      *= smoothstep(IN.Thickness + IN.Fade, IN.Thickness, dist);

    if (circleAlpha == 0.0)
        discard;

    OUT.Color    = IN.Color;
    OUT.Color.a *= circleAlpha;
    OUT.EntityID = IN.EntityID;
    return OUT;
}
