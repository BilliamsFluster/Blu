// PostProcess_FogVolume.hlsl — localized fog volumes.
// Reconstructs world position from the scene depth buffer, then for each volume integrates
// fog density along the view ray segment (camera -> reconstructed surface) where it passes
// through the box/sphere. Analytic (no ray-marching). Composited in HDR/linear space before
// tonemapping. Mirrors the depth-reconstruction convention used by SSAO.hlsl.
//   t0 = scene HDR color, t1 = scene depth (R24_UNORM_X8_TYPELESS), s0 = point sampler.

#pragma pack_matrix(column_major)

#define MAX_FOG_VOLUMES 16

struct FogVolume
{
    float4 PositionShape;  // xyz = world center,  w = shape (0 = box, 1 = sphere)
    float4 ExtentsDensity; // box half-extents (xyz) or sphere radius (x),  w = density per unit
    float4 ColorFalloff;   // rgb = fog color,  w = falloff (reserved)
};

cbuffer FogParams : register(b0)
{
    float4x4  u_InvViewProj;          // clip -> world
    float3    u_CameraPos; float u_VolumeCount;
    FogVolume u_Volumes[MAX_FOG_VOLUMES];
};

Texture2D        u_SceneTex : register(t0);
Texture2D<float> u_DepthTex : register(t1);
SamplerState     u_Samp     : register(s0);

// Reconstruct world position from UV + NDC depth (DX11 RH_ZO: Z in [0,1]). Same NDC/Y
// convention as SSAO.hlsl, but inverted by the full view-projection to land in world space.
float3 ReconstructWorldPos(float2 uv, float depth)
{
    float2 ndc   = float2(uv.x * 2.0 - 1.0, -(uv.y * 2.0 - 1.0));
    float4 clip  = float4(ndc, depth, 1.0);
    float4 world = mul(u_InvViewProj, clip);
    if (abs(world.w) < 1e-6) world.w = 1e-6;
    return world.xyz / world.w;
}

// Length of the ray (origin ro, unit dir rd, clamped to [0, segLen]) inside an axis-aligned box.
float BoxOverlap(float3 ro, float3 rd, float segLen, float3 center, float3 halfExt)
{
    float3 inv  = 1.0 / rd;            // rd is unit; zero components -> +/-inf, slab test still valid
    float3 t0   = (center - halfExt - ro) * inv;
    float3 t1   = (center + halfExt - ro) * inv;
    float3 tlo  = min(t0, t1);
    float3 thi  = max(t0, t1);
    float  tEnter = max(max(max(tlo.x, tlo.y), tlo.z), 0.0);
    float  tExit  = min(min(min(thi.x, thi.y), thi.z), segLen);
    return max(0.0, tExit - tEnter);
}

// Length of the ray inside a sphere, clamped to [0, segLen].
float SphereOverlap(float3 ro, float3 rd, float segLen, float3 center, float radius)
{
    float3 oc = ro - center;
    float  b  = dot(oc, rd);
    float  c  = dot(oc, oc) - radius * radius;
    float  disc = b * b - c;
    if (disc < 0.0) return 0.0;
    float  s = sqrt(disc);
    float  tEnter = max(-b - s, 0.0);
    float  tExit  = min(-b + s, segLen);
    return max(0.0, tExit - tEnter);
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
    float3 scene = u_SceneTex.SampleLevel(u_Samp, IN.TexCoord, 0).rgb;
    float  depth = u_DepthTex.SampleLevel(u_Samp, IN.TexCoord, 0);

    float3 worldPos = ReconstructWorldPos(IN.TexCoord, depth);
    float3 ro       = u_CameraPos;
    float3 toFrag   = worldPos - ro;
    float  segLen   = length(toFrag);
    float3 rd       = (segLen > 1e-4) ? toFrag / segLen : float3(0.0, 0.0, 1.0);

    float  totalDensity = 0.0;
    float3 fogColorAccum = float3(0.0, 0.0, 0.0);
    float  weightSum     = 0.0;

    int count = (int)min(u_VolumeCount, (float)MAX_FOG_VOLUMES);
    [loop]
    for (int i = 0; i < count; ++i)
    {
        FogVolume v = u_Volumes[i];
        float overlap = (v.PositionShape.w < 0.5)
            ? BoxOverlap   (ro, rd, segLen, v.PositionShape.xyz, v.ExtentsDensity.xyz)
            : SphereOverlap(ro, rd, segLen, v.PositionShape.xyz, v.ExtentsDensity.x);

        float d = overlap * max(v.ExtentsDensity.w, 0.0);  // density * length through the volume
        totalDensity  += d;
        fogColorAccum += v.ColorFalloff.rgb * d;
        weightSum     += d;
    }

    if (weightSum <= 1e-5)
        return float4(scene, 1.0);

    float3 fogColor      = fogColorAccum / weightSum;       // density-weighted blend of volume colors
    float  transmittance = exp(-totalDensity);
    float3 outColor      = scene * transmittance + fogColor * (1.0 - transmittance);
    return float4(outColor, 1.0);
}
