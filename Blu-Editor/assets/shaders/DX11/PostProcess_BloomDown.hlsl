// Bloom downsample: 13-tap Kawase-style box filter.
// Bind the source mip and render into the next-smaller mip FB.

cbuffer TexelSize : register(b0)
{
    float u_InvSrcW;
    float u_InvSrcH;
    float2 _pad;
};

Texture2D    u_SrcTexture  : register(t0);
SamplerState u_LinearSampler : register(s0);

#type vertex
struct VS_IN  { float2 a_Position : a_Position; float2 a_TexCoord : a_TexCoord; };
struct VS_OUT { float4 Position : SV_Position;  float2 TexCoord   : TEXCOORD; };
VS_OUT main(VS_IN IN)
{
    VS_OUT OUT;
    OUT.Position = float4(IN.a_Position, 0.0, 1.0);
    OUT.TexCoord = IN.a_TexCoord;
    return OUT;
}

#type pixel
struct PS_IN { float4 Position : SV_Position; float2 TexCoord : TEXCOORD; };

float4 main(PS_IN IN) : SV_Target
{
    float2 uv = IN.TexCoord;
    float2 p  = float2(u_InvSrcW, u_InvSrcH);

    // Centre 4-sample box (weight 0.5)
    float3 c  = u_SrcTexture.Sample(u_LinearSampler, uv + float2(-1,-1) * p).rgb;
    c += u_SrcTexture.Sample(u_LinearSampler, uv + float2( 1,-1) * p).rgb;
    c += u_SrcTexture.Sample(u_LinearSampler, uv + float2(-1, 1) * p).rgb;
    c += u_SrcTexture.Sample(u_LinearSampler, uv + float2( 1, 1) * p).rgb;
    c *= 0.5;

    // Outer ring of 8 at half-texel (weight 0.125 each, total 0.5)
    c += u_SrcTexture.Sample(u_LinearSampler, uv + float2(-2,-2) * p).rgb * 0.0625;
    c += u_SrcTexture.Sample(u_LinearSampler, uv + float2( 0,-2) * p).rgb * 0.125;
    c += u_SrcTexture.Sample(u_LinearSampler, uv + float2( 2,-2) * p).rgb * 0.0625;
    c += u_SrcTexture.Sample(u_LinearSampler, uv + float2(-2, 0) * p).rgb * 0.125;
    c += u_SrcTexture.Sample(u_LinearSampler, uv + float2( 2, 0) * p).rgb * 0.125;
    c += u_SrcTexture.Sample(u_LinearSampler, uv + float2(-2, 2) * p).rgb * 0.0625;
    c += u_SrcTexture.Sample(u_LinearSampler, uv + float2( 0, 2) * p).rgb * 0.125;
    c += u_SrcTexture.Sample(u_LinearSampler, uv + float2( 2, 2) * p).rgb * 0.0625;

    // Normalise: weights above sum to (4*0.125 + 4*0.0625 + 8*0.0625) = 1.0
    return float4(c, 1.0);
}
