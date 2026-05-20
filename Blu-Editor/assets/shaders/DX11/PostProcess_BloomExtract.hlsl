// Bloom extraction: sample the HDR scene, discard pixels below the luminance
// threshold, and output a 2x-downsampled bright-pass buffer.

cbuffer BloomParams : register(b0)
{
    float u_Threshold;   // HDR luminance threshold
    float u_InvSrcW;     // 1 / source width
    float u_InvSrcH;     // 1 / source height
    float _pad;
};

Texture2D    u_SceneTexture : register(t0);
SamplerState u_LinearSampler : register(s0);

// ── Vertex shader ──────────────────────────────────────────────────────────────
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

// ── Pixel shader ───────────────────────────────────────────────────────────────
#type pixel

struct PS_IN { float4 Position : SV_Position; float2 TexCoord : TEXCOORD; };

float Luminance(float3 c) { return dot(c, float3(0.2126, 0.7152, 0.0722)); }

float4 main(PS_IN IN) : SV_Target
{
    float2 uv  = IN.TexCoord;
    float2 off = float2(u_InvSrcW, u_InvSrcH);

    // 4-tap bilinear box at half-texel offsets for a clean 2x downsample
    float3 c  = u_SceneTexture.Sample(u_LinearSampler, uv + float2(-0.5, -0.5) * off).rgb;
    c += u_SceneTexture.Sample(u_LinearSampler, uv + float2( 0.5, -0.5) * off).rgb;
    c += u_SceneTexture.Sample(u_LinearSampler, uv + float2(-0.5,  0.5) * off).rgb;
    c += u_SceneTexture.Sample(u_LinearSampler, uv + float2( 0.5,  0.5) * off).rgb;
    c *= 0.25;

    // Soft threshold: smoothly ramp from 0 at (threshold-knee) to full at threshold+knee
    float knee = u_Threshold * 0.5;
    float luma = Luminance(c);
    float ramp = clamp((luma - (u_Threshold - knee)) / (2.0 * knee + 0.001), 0.0, 1.0);
    ramp = ramp * ramp;
    c *= ramp;

    return float4(c, 1.0);
}
