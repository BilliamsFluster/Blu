Texture2D    u_SceneTexture : register(t0);
SamplerState u_SceneSampler : register(s0);

#type vertex
struct VS_IN
{
    float2 a_Position : a_Position;
    float2 a_TexCoord : a_TexCoord;
};
struct VS_OUT
{
    float4 Position : SV_Position;
    float2 TexCoord : TEXCOORD;
};
VS_OUT main(VS_IN IN)
{
    VS_OUT OUT;
    OUT.Position = float4(IN.a_Position, 0.0, 1.0);
    OUT.TexCoord = IN.a_TexCoord;
    return OUT;
}

#type pixel
struct PS_IN
{
    float4 Position : SV_Position;
    float2 TexCoord : TEXCOORD;
};
float4 main(PS_IN IN) : SV_Target
{
    float3 color = u_SceneTexture.Sample(u_SceneSampler, IN.TexCoord).rgb;
    color = color / (color + float3(1.0, 1.0, 1.0));
    color = pow(color, float3(1.0 / 2.2, 1.0 / 2.2, 1.0 / 2.2));
    return float4(color, 1.0);
}
