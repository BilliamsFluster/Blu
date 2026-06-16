// PostProcess_Decals.hlsl — screen-space projected decals (bullet holes, scorch, blood).
// Reconstructs world position from the scene depth buffer (same convention as SSAO / fog
// volumes), transforms it into each decal's local unit-box space, and alpha-blends a radial
// (circular) decal onto surfaces inside the box. Composited in HDR/linear space before tonemap.
//   t0 = scene HDR color, t1 = scene depth (R24_UNORM_X8_TYPELESS), s0 = point sampler.

#pragma pack_matrix(column_major)

#define MAX_DECALS 32

struct Decal
{
    float4x4 InvWorld;       // world -> decal local (unit box centred at origin)
    float4   ColorOpacity;   // rgb = color, w = opacity
    float4   FalloffPad;     // x = falloff (soft-edge fraction), yzw unused
};

cbuffer DecalParams : register(b0)
{
    float4x4 u_InvViewProj;            // clip -> world
    float3   u_CameraPos; float u_DecalCount;
    Decal    u_Decals[MAX_DECALS];
};

Texture2D        u_SceneTex : register(t0);
Texture2D<float> u_DepthTex : register(t1);
SamplerState     u_Samp     : register(s0);

float3 ReconstructWorldPos(float2 uv, float depth)
{
    float2 ndc   = float2(uv.x * 2.0 - 1.0, -(uv.y * 2.0 - 1.0));
    float4 clip  = float4(ndc, depth, 1.0);
    float4 world = mul(u_InvViewProj, clip);
    if (abs(world.w) < 1e-6) world.w = 1e-6;
    return world.xyz / world.w;
}

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
    float3 color = u_SceneTex.SampleLevel(u_Samp, IN.TexCoord, 0).rgb;
    float  depth = u_DepthTex.SampleLevel(u_Samp, IN.TexCoord, 0);

    // Sky / cleared background has no surface to project onto.
    if (depth >= 0.99999)
        return float4(color, 1.0);

    float3 worldPos = ReconstructWorldPos(IN.TexCoord, depth);

    int count = (int)min(u_DecalCount, (float)MAX_DECALS);
    [loop]
    for (int i = 0; i < count; ++i)
    {
        Decal d = u_Decals[i];
        float3 local = mul(d.InvWorld, float4(worldPos, 1.0)).xyz; // decal-local space

        // Inside the unit box?
        float3 a = abs(local);
        if (a.x > 0.5 || a.y > 0.5 || a.z > 0.5)
            continue;

        // Radial decal in the box's local XZ plane (Y is the projection thickness).
        float r = length(local.xz) * 2.0;                // 0 at centre, 1 at box edge
        float falloff = saturate(d.FalloffPad.x);
        float edge = 1.0 - smoothstep(1.0 - falloff, 1.0, r); // soft circular edge
        float fadeY = 1.0 - smoothstep(0.3, 0.5, a.y);        // fade near top/bottom of the box
        float alpha = saturate(edge * fadeY * d.ColorOpacity.w);

        color = lerp(color, d.ColorOpacity.rgb, alpha);
    }

    return float4(color, 1.0);
}
