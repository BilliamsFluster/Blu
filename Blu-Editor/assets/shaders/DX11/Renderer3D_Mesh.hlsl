// Renderer3D_Mesh.hlsl
// Phong lighting for 3D mesh entities.
// Semantic names must match the engine's BufferElement.Name strings.
//
// Constant buffer layout mirrors Renderer3D::DrawMesh + PassLights calls.

// Matrices are uploaded column-major (raw GLM) — HLSL cbuffer default is also column-major,
// so no transpose is required. mul(M, v) computes the correct M*v.

// -----------------------------------------------------------------------
// Constant buffers
// -----------------------------------------------------------------------

cbuffer PerFrame : register(b0)
{
    float4x4 u_ViewProjectionMatrix;
    float3   u_ViewPos;
    float    _pad0;
};

cbuffer PerObject : register(b1)
{
    float4x4 u_Model;
    float3x3 u_NormalMatrix;   // padded to 3 x float4 = 48 bytes
    float    _pad1[3];
    int      u_EntityID;
    float3   _pad2;
};

struct Material
{
    float3 ambient;
    float  _p0;
    float3 diffuse;
    float  _p1;
    float3 specular;
    float  shininess;
};

struct Light
{
    float3 position;
    float  _p0;
    float3 ambient;
    float  _p1;
    float3 diffuse;
    float  _p2;
    float3 specular;
    float  _p3;
};

cbuffer MaterialData : register(b2)
{
    Material u_Material;
};

cbuffer LightData : register(b3)
{
    Light u_Lights[8];
    int   u_NumLights;
    float3 _padL;
};

// -----------------------------------------------------------------------
// Vertex shader
// -----------------------------------------------------------------------

#type vertex

struct VS_IN
{
    float3 a_Position : a_Position;
    float3 a_Normal   : a_Normal;
    float2 a_TexCoord : a_TexCoord;
};

struct VS_OUT
{
    float4 Position  : SV_Position;
    float3 FragPos   : FRAGPOS;
    float3 Normal    : NORMAL;
    float2 TexCoord  : TEXCOORD;
};

VS_OUT main(VS_IN IN)
{
    VS_OUT OUT;
    float4 worldPos = mul(u_Model, float4(IN.a_Position, 1.0));
    OUT.FragPos  = worldPos.xyz;
    OUT.Position = mul(u_ViewProjectionMatrix, worldPos);
    OUT.Normal   = normalize(mul((float3x3)u_NormalMatrix, IN.a_Normal));
    OUT.TexCoord = IN.a_TexCoord;
    return OUT;
}

// -----------------------------------------------------------------------
// Pixel shader
// -----------------------------------------------------------------------

#type pixel

struct PS_IN
{
    float4 Position : SV_Position;
    float3 FragPos  : FRAGPOS;
    float3 Normal   : NORMAL;
    float2 TexCoord : TEXCOORD;
};

struct PS_OUT
{
    float4 Color    : SV_Target0;
    int    EntityID : SV_Target1;
};

float3 CalcPointLight(Light light, float3 normal, float3 fragPos, float3 viewDir)
{
    float3 lightDir   = normalize(light.position - fragPos);
    float3 reflectDir = reflect(-lightDir, normal);

    float diff = max(dot(normal, lightDir), 0.0);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), u_Material.shininess);

    float3 ambient  = light.ambient  * u_Material.ambient;
    float3 diffuse  = light.diffuse  * diff * u_Material.diffuse;
    float3 specular = light.specular * spec * u_Material.specular;
    return ambient + diffuse + specular;
}

PS_OUT main(PS_IN IN)
{
    PS_OUT OUT;
    float3 normal  = normalize(IN.Normal);
    float3 viewDir = normalize(u_ViewPos - IN.FragPos);

    float3 result = float3(0, 0, 0);
    if (u_NumLights == 0)
    {
        // Unlit fallback
        result = u_Material.diffuse;
    }
    else
    {
        for (int i = 0; i < u_NumLights; i++)
            result += CalcPointLight(u_Lights[i], normal, IN.FragPos, viewDir);
    }

    OUT.Color    = float4(result, 1.0);
    OUT.EntityID = u_EntityID;
    return OUT;
}
