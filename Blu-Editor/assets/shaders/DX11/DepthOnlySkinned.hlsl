// Depth-only shader for SKINNED meshes — renders bone-deformed geometry into the
// cascaded shadow map so animated characters cast shadows. Mirrors DepthOnly.hlsl
// (same u_LightVP / u_Model cbuffers) but consumes the skinned vertex layout and the
// BoneData cbuffer (b6) exactly as Skinned_Mesh.hlsl does, so the skinning math used
// for the visible mesh and its shadow stay identical.
cbuffer PerFrame : register(b0)
{
    float4x4 u_LightVP;
};

cbuffer PerObject : register(b1)
{
    float4x4 u_Model;
};

cbuffer BoneData : register(b6)
{
    float4x4 u_Bones[128]; // 128 * 64 = 8 192 bytes — matches BoneDataGPU / Skinned_Mesh.hlsl
};

#type vertex
struct VS_IN
{
    float3  a_Position    : a_Position;
    float3  a_Normal      : a_Normal;
    float2  a_TexCoord    : a_TexCoord;
    float3  a_Tangent     : a_Tangent;
    int4    a_BoneIDs     : a_BoneIDs;
    float4  a_BoneWeights : a_BoneWeights;
};
struct VS_OUT
{
    float4 Position : SV_Position;
};
VS_OUT main(VS_IN IN)
{
    VS_OUT OUT;

    // Blended skinning matrix (same as Skinned_Mesh.hlsl).
    float4x4 skinMat = (float4x4)0;
    skinMat += IN.a_BoneWeights[0] * u_Bones[IN.a_BoneIDs[0]];
    skinMat += IN.a_BoneWeights[1] * u_Bones[IN.a_BoneIDs[1]];
    skinMat += IN.a_BoneWeights[2] * u_Bones[IN.a_BoneIDs[2]];
    skinMat += IN.a_BoneWeights[3] * u_Bones[IN.a_BoneIDs[3]];

    float wsum = IN.a_BoneWeights[0] + IN.a_BoneWeights[1]
               + IN.a_BoneWeights[2] + IN.a_BoneWeights[3];
    if (wsum < 0.001)
        skinMat = float4x4(1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1);

    float4 skinnedPos = mul(skinMat, float4(IN.a_Position, 1.0));
    OUT.Position = mul(u_LightVP, mul(u_Model, skinnedPos));
    return OUT;
}

#type pixel
float4 main() : SV_Target
{
    return 1.0f;
}
