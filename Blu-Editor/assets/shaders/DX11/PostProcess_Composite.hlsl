// Final composite pass: ACES filmic tonemap + bloom add + FXAA.
// Reads the full-res HDR scene + the half-res bloom layer, outputs LDR sRGB.

cbuffer CompositeParams : register(b0)
{
    float  u_BloomStrength;   // scalar applied to bloom before add
    float  u_EnableFXAA;      // 1 = enable, 0 = skip
    float  u_InvW;            // 1 / viewport width
    float  u_InvH;            // 1 / viewport height
    float  u_SSAOStrength;    // 0 = off, 1 = full AO darkening
    float3 _pad;
};

Texture2D    u_SceneTexture  : register(t0); // HDR scene
Texture2D    u_BloomTexture  : register(t1); // half-res bloom accumulation
Texture2D    u_AOTexture     : register(t2); // blurred SSAO [0=occluded, 1=clear]
SamplerState u_LinearSampler : register(s0);

// ── ACES filmic (Narkowicz 2015 fast approximation) ─────────────────────────────
float3 ACESFilmic(float3 x)
{
    const float a = 2.51f, b = 0.03f, c = 2.43f, d = 0.59f, e = 0.14f;
    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

// ── FXAA (classic 4-corner diagonal version) ───────────────────────────────────
float Luma(float3 rgb) { return dot(rgb, float3(0.299, 0.587, 0.114)); }

float3 FXAA(Texture2D tex, SamplerState samp, float2 uv, float2 inv)
{
    float SPAN_MAX = 8.0, REDUCE_MUL = 1.0 / 8.0, REDUCE_MIN = 1.0 / 128.0;

    float3 nw = tex.Sample(samp, uv + float2(-1,-1) * inv).rgb;
    float3 ne = tex.Sample(samp, uv + float2( 1,-1) * inv).rgb;
    float3 sw = tex.Sample(samp, uv + float2(-1, 1) * inv).rgb;
    float3 se = tex.Sample(samp, uv + float2( 1, 1) * inv).rgb;
    float3 m  = tex.Sample(samp, uv).rgb;

    float lumaNW = Luma(nw), lumaNE = Luma(ne);
    float lumaSW = Luma(sw), lumaSE = Luma(se);
    float lumaM  = Luma(m);

    float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
    float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));

    float2 dir = float2(
        -((lumaNW + lumaNE) - (lumaSW + lumaSE)),
         ((lumaNW + lumaSW) - (lumaNE + lumaSE)));

    float dirReduce = max((lumaNW+lumaNE+lumaSW+lumaSE) * 0.25 * REDUCE_MUL, REDUCE_MIN);
    float rcpDirMin = 1.0 / (min(abs(dir.x), abs(dir.y)) + dirReduce);
    dir = clamp(dir * rcpDirMin, -SPAN_MAX, SPAN_MAX) * inv;

    float3 A = 0.5 * (
        tex.Sample(samp, uv + dir * (1.0/3.0 - 0.5)).rgb +
        tex.Sample(samp, uv + dir * (2.0/3.0 - 0.5)).rgb);
    float3 B = A * 0.5 + 0.25 * (
        tex.Sample(samp, uv + dir * -0.5).rgb +
        tex.Sample(samp, uv + dir *  0.5).rgb);

    float lumaB = Luma(B);
    return (lumaB < lumaMin || lumaB > lumaMax) ? A : B;
}

// ── Vertex shader ────────────────────────────────────────────────────────────────
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

// ── Pixel shader ─────────────────────────────────────────────────────────────────
#type pixel
struct PS_IN { float4 Position : SV_Position; float2 TexCoord : TEXCOORD; };

float4 main(PS_IN IN) : SV_Target
{
    float2 uv  = IN.TexCoord;
    float2 inv = float2(u_InvW, u_InvH);

    // Optionally apply FXAA (operates on raw HDR; luminance differences are the same)
    float3 hdr;
    if (u_EnableFXAA > 0.5)
        hdr = FXAA(u_SceneTexture, u_LinearSampler, uv, inv);
    else
        hdr = u_SceneTexture.Sample(u_LinearSampler, uv).rgb;

    // Add bloom
    float3 bloom = u_BloomTexture.Sample(u_LinearSampler, uv).rgb;
    hdr += bloom * u_BloomStrength;

    // SSAO — multiply into scene before tonemapping (dims indirect light)
    if (u_SSAOStrength > 0.0)
    {
        float ao = u_AOTexture.Sample(u_LinearSampler, uv).r;
        hdr *= lerp(1.0, ao, u_SSAOStrength);
    }

    // ACES filmic tonemap
    float3 ldr = ACESFilmic(hdr);

    // Gamma correction (linear → sRGB approximation)
    ldr = pow(max(ldr, 0.0), 1.0 / 2.2);

    return float4(ldr, 1.0);
}
