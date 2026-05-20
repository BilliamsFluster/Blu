// SSAO.hlsl — Screen-Space Ambient Occlusion
// Reconstructs view-space normals from depth derivatives, then tests a
// hemisphere kernel to compute ambient occlusion [0=fully occluded, 1=clear].
// t0 = scene depth (R24_UNORM_X8_TYPELESS), s0 = point sampler.

#pragma pack_matrix(column_major)

cbuffer SSAOParams : register(b0)
{
    float4x4 u_Projection;      // 64 bytes
    float4x4 u_InvProjection;   // 64 bytes
    float    u_Radius;          // view-space hemisphere radius
    float    u_Bias;            // depth bias to avoid self-occlusion
    float    u_Power;           // AO contrast exponent
    float    u_TexelW;          // 1 / viewport width
    float    u_TexelH;          // 1 / viewport height
    int      u_NumSamples;      // active sample count [1..32]
    float2   _pad;              // → 160 bytes total before kernel
    float4   u_Kernel[32];      // tangent-space hemisphere samples (xyz + w unused)
};

Texture2D<float> u_DepthTex  : register(t0);
SamplerState     u_PointSamp : register(s0);

// Reconstruct view-space position from UV + NDC depth (DX11 RH_ZO: Z in [0,1]).
float3 ReconstructViewPos(float2 uv, float depth)
{
    float2 ndc    = float2(uv.x * 2.0 - 1.0, -(uv.y * 2.0 - 1.0));
    float4 clip4  = float4(ndc, depth, 1.0);
    float4 viewP  = mul(u_InvProjection, clip4);
    return viewP.xyz / viewP.w;
}

// Deterministic per-pixel tangent-space rotation vector via screen-space hash.
float3 RandomTangent(float2 uv)
{
    float  h  = frac(sin(dot(uv * 4096.0, float2(127.1, 311.7))) * 43758.5453);
    float  h2 = frac(sin(dot(uv * 4096.0, float2(269.5, 183.3))) * 43758.5453);
    return normalize(float3(h * 2.0 - 1.0, h2 * 2.0 - 1.0, 0.0));
}

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
    float2 uv    = IN.TexCoord;
    float  depth = u_DepthTex.SampleLevel(u_PointSamp, uv, 0);

    // Sky / background — no geometry, return fully unoccluded.
    if (depth >= 0.9999) return float4(1, 1, 1, 1);

    float3 fragPos = ReconstructViewPos(uv, depth);

    // Reconstruct view-space normal from neighbouring depth samples.
    // Moving +U on screen = +X in view; moving -V on screen = +Y in view (DX11 Y-down).
    float2 tw = float2(u_TexelW, 0);
    float2 th = float2(0, u_TexelH);
    float3 posR = ReconstructViewPos(saturate(uv + tw), u_DepthTex.SampleLevel(u_PointSamp, saturate(uv + tw), 0));
    float3 posU = ReconstructViewPos(saturate(uv - th), u_DepthTex.SampleLevel(u_PointSamp, saturate(uv - th), 0));
    float3 N    = normalize(cross(posR - fragPos, posU - fragPos));

    // Build TBN frame to orient the kernel along N.
    float3 randVec = RandomTangent(uv);
    float3 T = normalize(randVec - N * dot(randVec, N));
    float3 B = cross(N, T);

    float occlusion = 0.0;
    int   count     = clamp(u_NumSamples, 1, 32);

    [loop]
    for (int i = 0; i < count; ++i)
    {
        // Kernel sample in tangent space → view space
        float3 kv       = u_Kernel[i].xyz;
        float3 sampleVS = T * kv.x + B * kv.y + N * kv.z;
        float3 samplePos = fragPos + sampleVS * u_Radius;

        // Project sample back to UV space
        float4 sampleClip = mul(u_Projection, float4(samplePos, 1.0));
        sampleClip.xyz   /= sampleClip.w;
        float2 sampleUV   = float2(sampleClip.x * 0.5 + 0.5, -sampleClip.y * 0.5 + 0.5);
        sampleUV          = saturate(sampleUV);

        // Depth at sample location → view-space Z of the actual geometry
        float  sd       = u_DepthTex.SampleLevel(u_PointSamp, sampleUV, 0);
        float3 sGeomPos = ReconstructViewPos(sampleUV, sd);

        // RH: view-space Z is negative; more negative = farther from camera.
        // Geometry is occluding if it is closer (less negative Z) than our sample.
        float rangeCheck = smoothstep(0.0, 1.0, u_Radius / max(0.001, abs(fragPos.z - sGeomPos.z)));
        occlusion += (sGeomPos.z >= samplePos.z + u_Bias ? 1.0 : 0.0) * rangeCheck;
    }

    float ao = 1.0 - (occlusion / (float)count);
    ao = pow(saturate(ao), u_Power);
    return float4(ao, ao, ao, 1.0);
}
