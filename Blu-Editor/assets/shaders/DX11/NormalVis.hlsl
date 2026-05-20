cbuffer PerFrame : register(b0)
{
    float4x4 u_ViewProjectionMatrix;
};

cbuffer PerObject : register(b1)
{
    float4x4 u_Model;
};

#type vertex
struct VS_IN
{
    float3 a_Position : a_Position;
    float3 a_Normal   : a_Normal;
    float2 a_TexCoord : a_TexCoord;
};
struct VS_OUT
{
    float4 Position : SV_Position;
    float3 WorldPos : WORLDPOS;
    float3 Normal   : NORMAL;
};
VS_OUT main(VS_IN IN)
{
    VS_OUT OUT;
    float4 worldPos = mul(u_Model, float4(IN.a_Position, 1.0));
    OUT.WorldPos = worldPos.xyz;
    OUT.Position = mul(u_ViewProjectionMatrix, worldPos);
    OUT.Normal   = normalize(mul((float3x3)u_Model, IN.a_Normal));
    return OUT;
}

#type geometry
struct GS_IN
{
    float4 Position : SV_Position;
    float3 WorldPos : WORLDPOS;
    float3 Normal   : NORMAL;
};
struct GS_OUT
{
    float4 Position : SV_Position;
    float3 Color    : COLOR;
};
[maxvertexcount(6)]
void main(triangle GS_IN input[3], inout LineStream<GS_OUT> stream)
{
    float normalLen = 0.3f;
    for (uint i = 0; i < 3; i++)
    {
        GS_OUT v;
        v.Position = mul(u_ViewProjectionMatrix, float4(input[i].WorldPos, 1.0));
        v.Color = float3(0.0, 1.0, 0.0);
        stream.Append(v);

        v.Position = mul(u_ViewProjectionMatrix, float4(input[i].WorldPos + normalize(input[i].Normal) * normalLen, 1.0));
        v.Color = float3(1.0, 0.0, 0.0);
        stream.Append(v);

        stream.RestartStrip();
    }
}

#type pixel
struct PS_IN
{
    float4 Position : SV_Position;
    float3 Color    : COLOR;
};
float4 main(PS_IN IN) : SV_Target
{
    return float4(IN.Color, 1.0);
}
