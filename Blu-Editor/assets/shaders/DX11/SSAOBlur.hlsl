// SSAOBlur.hlsl — 5x5 box blur to smooth SSAO noise.
// t0 = raw SSAO texture, s0 = point sampler.

cbuffer BlurParams : register(b0)
{
    float u_InvW;
    float u_InvH;
    float2 _pad;
};

Texture2D<float4> u_AOTex    : register(t0);
SamplerState      u_PointSamp : register(s0);

// ── Vertex shader ─────────────────────────────────────────────────────────────
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

// ── Pixel shader ─────────────────────────────────────────────────────────────
#type pixel
struct PS_IN { float4 Position : SV_Position; float2 TexCoord : TEXCOORD; };

float4 main(PS_IN IN) : SV_Target
{
    float2 uv  = IN.TexCoord;
    float2 inv = float2(u_InvW, u_InvH);

    float ao = 0.0;
    [unroll]
    for (int x = -2; x <= 2; ++x)
    [unroll]
    for (int y = -2; y <= 2; ++y)
        ao += u_AOTex.SampleLevel(u_PointSamp, uv + float2(x, y) * inv, 0).r;

    ao /= 25.0;
    return float4(ao, ao, ao, 1.0);
}
