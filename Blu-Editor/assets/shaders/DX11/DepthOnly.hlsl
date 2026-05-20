cbuffer PerFrame : register(b0)
{
    float4x4 u_LightVP;
};

cbuffer PerObject : register(b1)
{
    float4x4 u_Model;
};

#type vertex
struct VS_IN
{
    float3 a_Position : a_Position;
};
struct VS_OUT
{
    float4 Position : SV_Position;
};
VS_OUT main(VS_IN IN)
{
    VS_OUT OUT;
    OUT.Position = mul(u_LightVP, mul(u_Model, float4(IN.a_Position, 1.0)));
    return OUT;
}

#type pixel
float4 main() : SV_Target
{
    return 1.0f;
}
