cbuffer DeferredFrame : register(b0)
{
    float3 u_ViewPos; int u_HasShadowMap;
    float3 u_FogColor; float u_FogDensity;
    float u_FogHeightStart; float u_FogHeightDensity;
    int u_FogEnabled; float u_AerialStrength;
    float3 u_AerialColor; float _fogPad;
};

struct DirectionalLight
{
    float3 Direction; float _p0;
    float3 Ambient; float Intensity;
    float3 Diffuse; float _p1;
    float3 Specular; float _p2;
};

struct PointLight
{
    float3 Position; float Range;
    float3 Ambient; float Intensity;
    float3 Diffuse; float _p0;
    float3 Specular; float _p1;
    float3 Att; float _p2;
};

struct SpotLight
{
    float3 Position; float Range;
    float3 Direction; float Intensity;
    float3 Ambient; float _p0;
    float3 Diffuse; float _p1;
    float3 Specular; float _p2;
    float3 Att; float InnerCutoff;
    float OuterCutoff; float3 _p3;
};

cbuffer LightData : register(b3)
{
    DirectionalLight u_DirLights[4];
    PointLight u_PointLights[32];
    SpotLight u_SpotLights[4];
    int u_NumDirLights;
    int u_NumPointLights;
    int u_NumSpotLights;
    float _padLights;
};

cbuffer ShadowData : register(b4)
{
    float4x4 u_LightVPs[3];
    float3 u_CascadeSplits;
    float u_ShadowMapSize;
};

cbuffer IBLData : register(b6)
{
    int u_IBLEnabled;
    float u_IBLStrength;
    int u_IBLMipLevels;
    float _iblPad;
};

Texture2D u_GPosition : register(t0);
Texture2D u_GNormal : register(t1);
Texture2D u_GAlbedoAO : register(t2);
Texture2D u_GMaterial : register(t3);
Texture2D u_GEmissive : register(t4);
Texture2DArray u_ShadowMapArray : register(t5);
TextureCube u_IrradianceMap : register(t6);
TextureCube u_PrefilteredEnv : register(t7);
Texture2D u_BRDFLUT : register(t8);
Texture2D<int> u_GEntityID : register(t9);
SamplerState u_LinearSampler : register(s0);
SamplerComparisonState u_ShadowSampler : register(s1);

float D_GGX(float nDotH, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float d = nDotH * nDotH * (a2 - 1.0) + 1.0;
    return a2 / (3.14159 * d * d);
}

float3 F_Schlick(float cosTheta, float3 f0)
{
    return f0 + (1.0 - f0) * pow(saturate(1.0 - cosTheta), 5.0);
}

float G_SchlickGGX(float nDotV, float roughness)
{
    float k = (roughness + 1.0) * (roughness + 1.0) / 8.0;
    return nDotV / (nDotV * (1.0 - k) + k);
}

float G_Smith(float nDotV, float nDotL, float roughness)
{
    return G_SchlickGGX(nDotV, roughness) * G_SchlickGGX(nDotL, roughness);
}

float3 EvaluateBRDF(float3 normal, float3 viewDirection, float3 lightDirection,
                    float3 albedo, float metallic, float roughness)
{
    float3 halfway = normalize(viewDirection + lightDirection);
    float nDotV = max(dot(normal, viewDirection), 0.001);
    float nDotL = max(dot(normal, lightDirection), 0.001);
    float nDotH = max(dot(normal, halfway), 0.0);
    float hDotV = max(dot(halfway, viewDirection), 0.0);
    float3 f0 = lerp(float3(0.04, 0.04, 0.04), albedo, metallic);
    float3 fresnel = F_Schlick(hDotV, f0);
    float3 specular = D_GGX(nDotH, roughness) * fresnel * G_Smith(nDotV, nDotL, roughness)
        / max(4.0 * nDotV * nDotL, 0.001);
    float3 diffuse = (albedo / 3.14159) * (1.0 - fresnel) * (1.0 - metallic);
    return (diffuse + specular) * nDotL;
}

float CalcCSMShadowFactor(float3 fragPos)
{
    float distanceToCamera = distance(fragPos, u_ViewPos);
    int cascade = distanceToCamera < u_CascadeSplits.x ? 0
        : distanceToCamera < u_CascadeSplits.y ? 1 : 2;
    float4 lightPosition = mul(u_LightVPs[cascade], float4(fragPos, 1.0));
    float3 projection = lightPosition.xyz / lightPosition.w;
    float2 uv = float2(projection.x * 0.5 + 0.5, -projection.y * 0.5 + 0.5);
    if (projection.z < 0.0 || projection.z > 1.0 || any(uv < 0.0) || any(uv > 1.0))
        return 1.0;

    float texelSize = 1.0 / u_ShadowMapSize;
    float shadow = 0.0;
    [unroll] for (int x = -1; x <= 1; ++x)
        [unroll] for (int y = -1; y <= 1; ++y)
            shadow += u_ShadowMapArray.SampleCmpLevelZero(
                u_ShadowSampler, float3(uv + float2(x, y) * texelSize, (float)cascade), projection.z);
    return shadow / 9.0;
}

#type vertex

struct VS_IN
{
    float2 a_Position : a_Position;
    float2 a_TexCoord : a_TexCoord;
};

struct VS_OUT
{
    float4 Position : SV_Position;
    float2 TexCoord : TEXCOORD;
};

VS_OUT main(VS_IN input)
{
    VS_OUT output;
    output.Position = float4(input.a_Position, 0.0, 1.0);
    output.TexCoord = input.a_TexCoord;
    return output;
}

#type pixel

struct PS_IN
{
    float4 Position : SV_Position;
    float2 TexCoord : TEXCOORD;
};

struct PS_OUT
{
    float4 Color : SV_Target0;
    int EntityID : SV_Target1;
};

PS_OUT main(PS_IN input)
{
    int3 location = int3((int2)input.Position.xy, 0);
    float4 worldPosition = u_GPosition.Load(location);
    PS_OUT output;
    output.EntityID = u_GEntityID.Load(location);
    if (worldPosition.w < 0.5)
    {
        output.Color = float4(0.0, 0.0, 0.0, 1.0);
        return output;
    }

    float3 normal = normalize(u_GNormal.Load(location).xyz);
    float4 albedoAO = u_GAlbedoAO.Load(location);
    float4 material = u_GMaterial.Load(location);
    float3 emissive = u_GEmissive.Load(location).xyz;
    float3 albedo = albedoAO.rgb;
    float ao = albedoAO.a;
    float metallic = material.x;
    float roughness = material.y;

    if (material.z > 0.5)
    {
        output.Color = float4(albedo + emissive, material.w);
        return output;
    }

    float3 viewDirection = normalize(u_ViewPos - worldPosition.xyz);
    float3 result = 0.0;
    float shadow = u_HasShadowMap ? CalcCSMShadowFactor(worldPosition.xyz) : 1.0;

    for (int i = 0; i < u_NumDirLights; ++i)
    {
        float3 lightDirection = normalize(-u_DirLights[i].Direction);
        result += EvaluateBRDF(normal, viewDirection, lightDirection, albedo, metallic, roughness)
            * u_DirLights[i].Diffuse * u_DirLights[i].Intensity * shadow;
    }

    for (int i = 0; i < u_NumPointLights; ++i)
    {
        float3 toLight = u_PointLights[i].Position - worldPosition.xyz;
        float lightDistance = length(toLight);
        if (lightDistance <= u_PointLights[i].Range)
        {
            float attenuation = 1.0 / dot(u_PointLights[i].Att, float3(1.0, lightDistance, lightDistance * lightDistance));
            result += EvaluateBRDF(normal, viewDirection, toLight / lightDistance, albedo, metallic, roughness)
                * u_PointLights[i].Diffuse * u_PointLights[i].Intensity * attenuation;
        }
    }

    for (int i = 0; i < u_NumSpotLights; ++i)
    {
        float3 toLight = u_SpotLights[i].Position - worldPosition.xyz;
        float lightDistance = length(toLight);
        if (lightDistance <= u_SpotLights[i].Range)
        {
            float3 lightDirection = toLight / lightDistance;
            float attenuation = 1.0 / dot(u_SpotLights[i].Att, float3(1.0, lightDistance, lightDistance * lightDistance));
            float epsilon = max(u_SpotLights[i].InnerCutoff - u_SpotLights[i].OuterCutoff, 0.0001);
            float cone = saturate((dot(lightDirection, normalize(-u_SpotLights[i].Direction))
                - u_SpotLights[i].OuterCutoff) / epsilon);
            result += EvaluateBRDF(normal, viewDirection, lightDirection, albedo, metallic, roughness)
                * u_SpotLights[i].Diffuse * u_SpotLights[i].Intensity * attenuation * cone;
        }
    }

    float3 f0 = lerp(float3(0.04, 0.04, 0.04), albedo, metallic);
    if (u_IBLEnabled)
    {
        float3 fresnel = F_Schlick(max(dot(normal, viewDirection), 0.0), f0);
        float3 diffuseIBL = (1.0 - fresnel) * (1.0 - metallic)
            * u_IrradianceMap.Sample(u_LinearSampler, normal).rgb * albedo;
        float3 reflected = reflect(-viewDirection, normal);
        float3 prefiltered = u_PrefilteredEnv.SampleLevel(
            u_LinearSampler, reflected, roughness * (float)(u_IBLMipLevels - 1)).rgb;
        float2 brdf = u_BRDFLUT.Sample(u_LinearSampler, float2(max(dot(normal, viewDirection), 0.0), roughness)).rg;
        result += (diffuseIBL + prefiltered * (f0 * brdf.x + brdf.y)) * ao * u_IBLStrength;
    }
    else
    {
        result += (u_NumDirLights > 0
            ? u_DirLights[0].Ambient * u_DirLights[0].Intensity
            : float3(0.03, 0.03, 0.03)) * albedo * ao;
    }

    result += emissive;
    if (u_FogEnabled)
    {
        float distanceToCamera = length(u_ViewPos - worldPosition.xyz);
        float fogFactor = exp(-u_FogDensity * distanceToCamera);
        if (u_FogHeightDensity > 0.0)
            fogFactor *= exp(-max(0.0, worldPosition.y - u_FogHeightStart) * u_FogHeightDensity);
        float horizon = saturate(1.0 - abs(normalize(worldPosition.xyz - u_ViewPos).y));
        result = lerp(lerp(u_FogColor, u_AerialColor, u_AerialStrength * horizon), result, saturate(fogFactor));
    }

    output.Color = float4(result, material.w);
    return output;
}
