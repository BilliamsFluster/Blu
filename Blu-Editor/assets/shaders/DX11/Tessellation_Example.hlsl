cbuffer PerFrame : register(b0)
{
    float4x4 u_ViewProjectionMatrix;
};

cbuffer PerObject : register(b1)
{
    float4x4 u_Model;
};

#type vertex
struct VS_IN
{
    float3 a_Position : a_Position;
    float3 a_Normal   : a_Normal;
    float2 a_TexCoord : a_TexCoord;
};
struct VS_OUT
{
    float3 WorldPos : WORLDPOS;
    float3 Normal   : NORMAL;
    float2 TexCoord : TEXCOORD;
};
VS_OUT main(VS_IN IN)
{
    VS_OUT OUT;
    float4 worldPos = mul(u_Model, float4(IN.a_Position, 1.0));
    OUT.WorldPos = worldPos.xyz;
    OUT.Normal   = normalize(mul((float3x3)u_Model, IN.a_Normal));
    OUT.TexCoord = IN.a_TexCoord;
    return OUT;
}

#type hull
struct HS_CONSTANT_OUT
{
    float EdgeTessFactor[3] : SV_TessFactor;
    float InsideTessFactor  : SV_InsideTessFactor;
};
HS_CONSTANT_OUT ConstantHS()
{
    HS_CONSTANT_OUT OUT;
    OUT.EdgeTessFactor[0] = 4.0;
    OUT.EdgeTessFactor[1] = 4.0;
    OUT.EdgeTessFactor[2] = 4.0;
    OUT.InsideTessFactor  = 4.0;
    return OUT;
}
struct HS_OUT
{
    float3 WorldPos : WORLDPOS;
    float3 Normal   : NORMAL;
    float2 TexCoord : TEXCOORD;
};
[domain("tri")]
[partitioning("integer")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(3)]
[patchconstantfunc("ConstantHS")]
HS_OUT main(InputPatch<VS_OUT, 3> input, uint i : SV_OutputControlPointID)
{
    HS_OUT OUT;
    OUT.WorldPos = input[i].WorldPos;
    OUT.Normal   = input[i].Normal;
    OUT.TexCoord = input[i].TexCoord;
    return OUT;
}

#type domain
struct DS_OUT
{
    float4 Position : SV_Position;
    float3 WorldPos : WORLDPOS;
    float3 Normal   : NORMAL;
    float2 TexCoord : TEXCOORD;
};
[domain("tri")]
DS_OUT main(
    HS_CONSTANT_OUT input,
    float3 bary : SV_DomainLocation,
    const OutputPatch<HS_OUT, 3> tri)
{
    DS_OUT OUT;
    float3 worldPos = bary.x * tri[0].WorldPos + bary.y * tri[1].WorldPos + bary.z * tri[2].WorldPos;
    float3 normal   = bary.x * tri[0].Normal   + bary.y * tri[1].Normal   + bary.z * tri[2].Normal;
    float2 texCoord = bary.x * tri[0].TexCoord + bary.y * tri[1].TexCoord + bary.z * tri[2].TexCoord;

    OUT.Position = mul(u_ViewProjectionMatrix, float4(worldPos, 1.0));
    OUT.WorldPos = worldPos;
    OUT.Normal   = normalize(normal);
    OUT.TexCoord = texCoord;
    return OUT;
}

#type pixel
struct PS_IN
{
    float4 Position : SV_Position;
    float3 WorldPos : WORLDPOS;
    float3 Normal   : NORMAL;
    float2 TexCoord : TEXCOORD;
};
float4 main(PS_IN IN) : SV_Target
{
    float3 N = normalize(IN.Normal);
    float3 lightDir = normalize(float3(1.0, 1.0, 1.0));
    float diff = max(dot(N, lightDir), 0.0);
    return float4(diff.xxx, 1.0);
}
