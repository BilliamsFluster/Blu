// Bloom upsample: 9-tap tent filter.
// Render into the next-larger mip with ADDITIVE blending to accumulate bloom.

cbuffer TexelSize : register(b0)
{
    float u_InvSrcW;
    float u_InvSrcH;
    float u_FilterRadius; // 0.5 by default
    float _pad;
};

Texture2D    u_SrcTexture   : register(t0);
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
    float  r  = u_FilterRadius;
    float2 p  = float2(u_InvSrcW, u_InvSrcH);

    // 3x3 tent kernel (bilinear hardware does the heavy lifting)
    float3 c  = u_SrcTexture.Sample(u_LinearSampler, uv + float2(-r,-r) * p).rgb * 1.0;
    c += u_SrcTexture.Sample(u_LinearSampler, uv + float2( 0,-r) * p).rgb * 2.0;
    c += u_SrcTexture.Sample(u_LinearSampler, uv + float2( r,-r) * p).rgb * 1.0;
    c += u_SrcTexture.Sample(u_LinearSampler, uv + float2(-r, 0) * p).rgb * 2.0;
    c += u_SrcTexture.Sample(u_LinearSampler, uv + float2( 0, 0) * p).rgb * 4.0;
    c += u_SrcTexture.Sample(u_LinearSampler, uv + float2( r, 0) * p).rgb * 2.0;
    c += u_SrcTexture.Sample(u_LinearSampler, uv + float2(-r, r) * p).rgb * 1.0;
    c += u_SrcTexture.Sample(u_LinearSampler, uv + float2( 0, r) * p).rgb * 2.0;
    c += u_SrcTexture.Sample(u_LinearSampler, uv + float2( r, r) * p).rgb * 1.0;
    c /= 16.0;

    return float4(c, 1.0);
}
