#pragma pack_matrix(column_major)

// ─── Vertex shader ────────────────────────────────────────────────────────────
#type vertex

struct VS_Out
{
    float4 Position : SV_Position;
    float2 NDC      : TEXCOORD0;
};

// Full-screen triangle from SV_VertexID — no vertex buffer needed.
// z=w=1 so NDC depth=1 → sky fills only pixels that have no geometry.
VS_Out main(uint id : SV_VertexID)
{
    VS_Out OUT;
    OUT.NDC.x    = (id == 2) ?  3.0 : -1.0;
    OUT.NDC.y    = (id == 1) ?  3.0 : -1.0;
    OUT.Position = float4(OUT.NDC, 1.0, 1.0);
    return OUT;
}

// ─── Pixel shader ─────────────────────────────────────────────────────────────
#type pixel

cbuffer SkyboxCB : register(b0)
{
    float4x4 u_InvView;           // 64 — col3 = camera world pos
    float4x4 u_InvProjection;     // 64

    float3   u_GroundColor;    float  u_Turbidity;        // 16
    float3   u_SunDir;         float  u_SkyExposure;      // 16
    float3   u_SunColor;       float  u_SunSize;          // 16
    float    u_SunStrength;    float3 _pad0;              // 16

    float3   u_CloudColor;     float  u_CloudCoverage;    // 16
    float    u_CloudDensity;   float  u_CloudHeight;      // 8
    float    u_CloudScale;     float  u_Time;             // 8
    float    u_CloudScrollSpeed; float3 _cloudPad;        // 16
};

struct PS_In  { float4 Position : SV_Position; float2 NDC : TEXCOORD0; };
struct PS_Out { float4 Color : SV_Target0; int EntityID : SV_Target1; };

// ─────────────────────────────────────────────────────────────────────────────
// Preetham Sky Model — "A Practical Analytic Model for Daylight" (1999)
// Outputs CIE xyY → linear sRGB in kcd/m².  Scale by u_SkyExposure for tone.
// Valid turbidity range: ~1.8–10.  Values below 1.8 produce invalid results.
// ─────────────────────────────────────────────────────────────────────────────

static const float PI = 3.14159265f;

float PerezF(float theta, float gamma,
             float A, float B, float C, float D, float E)
{
    float cosTheta = max(cos(theta), 1e-4f);
    float cosGamma = cos(gamma);
    return (1.0f + A * exp(B / cosTheta))
         * (1.0f + C * exp(D * gamma) + E * cosGamma * cosGamma);
}

float3 PreethamSky(float3 viewDir, float3 sunDir, float T)
{
    T = clamp(T, 1.8f, 10.0f);   // model breaks below 1.8
    float T2 = T * T;

    float thetaSun = acos(clamp(sunDir.y,  0.0f, 1.0f));
    float theta    = acos(clamp(viewDir.y, 0.0f, 1.0f));
    float gamma    = acos(clamp(dot(viewDir, sunDir), -1.0f, 1.0f));

    float aY =  0.1787f*T - 1.4630f,  bY = -0.3554f*T + 0.4275f;
    float cY = -0.0227f*T + 5.3251f,  dY =  0.1206f*T - 2.5771f;
    float eY = -0.0670f*T + 0.3703f;

    float ax = -0.0193f*T - 0.2592f,  bx = -0.0665f*T + 0.0008f;
    float cx = -0.0004f*T + 0.2125f,  dx = -0.0641f*T - 0.8989f;
    float ex = -0.0033f*T + 0.0452f;

    float ay = -0.0167f*T - 0.2608f,  by = -0.0950f*T + 0.0092f;
    float cy = -0.0079f*T + 0.2102f,  dy = -0.0441f*T - 1.6537f;
    float ey = -0.0109f*T + 0.0529f;

    float chi = (4.0f/9.0f - T/120.0f) * (PI - 2.0f * thetaSun);
    float Yz  = max((4.0453f*T - 4.9710f) * tan(chi) - 0.2155f*T + 2.4192f, 0.0f);

    float ts = thetaSun, ts2 = ts*ts, ts3 = ts2*ts;

    float xz = ( 0.00166f*ts3 - 0.00375f*ts2 + 0.00209f*ts) * T2
             + (-0.02903f*ts3 + 0.06377f*ts2 - 0.03202f*ts + 0.00394f) * T
             + ( 0.11693f*ts3 - 0.21196f*ts2 + 0.06052f*ts + 0.25886f);

    float yz = ( 0.00275f*ts3 - 0.00610f*ts2 + 0.00317f*ts) * T2
             + (-0.04212f*ts3 + 0.08970f*ts2 - 0.04153f*ts + 0.00516f) * T
             + ( 0.15346f*ts3 - 0.26756f*ts2 + 0.06670f*ts + 0.26688f);

    float F0Y = PerezF(0.0f, thetaSun, aY, bY, cY, dY, eY);
    float F0x = PerezF(0.0f, thetaSun, ax, bx, cx, dx, ex);
    float F0y = PerezF(0.0f, thetaSun, ay, by, cy, dy, ey);

    float Y = Yz * PerezF(theta, gamma, aY, bY, cY, dY, eY) / max(F0Y, 1e-5f);
    float x = xz * PerezF(theta, gamma, ax, bx, cx, dx, ex) / max(F0x, 1e-5f);
    float y = yz * PerezF(theta, gamma, ay, by, cy, dy, ey) / max(F0y, 1e-5f);

    float yc = max(y, 1e-5f);
    float X  = Y * x / yc;
    float Z  = Y * (1.0f - x - y) / yc;

    float3 rgb;
    rgb.r =  3.2404542f*X - 1.5371385f*Y - 0.4985314f*Z;
    rgb.g = -0.9692660f*X + 1.8760108f*Y + 0.0415560f*Z;
    rgb.b =  0.0556434f*X - 0.2040259f*Y + 1.0572252f*Z;
    return max(rgb, 0.0f);
}

// ─── Improved procedural clouds ───────────────────────────────────────────────

float hash21(float2 p)
{
    p = frac(p * float2(127.1f, 311.7f));
    p += dot(p, p + 19.19f);
    return frac(p.x * p.y);
}

float smoothNoise(float2 p)
{
    float2 i = floor(p), f = frac(p);
    f = f * f * (3.0f - 2.0f * f);
    return lerp(lerp(hash21(i),               hash21(i + float2(1,0)), f.x),
                lerp(hash21(i + float2(0,1)), hash21(i + float2(1,1)), f.x), f.y);
}

float fbm(float2 p)
{
    float v = 0.0f, a = 0.5f;
    [unroll] for (int i = 0; i < 5; ++i) { v += a * smoothNoise(p); p *= 2.1f; a *= 0.5f; }
    return v / 0.96875f;
}

// Single-level domain warp — organic cloud billows without excessive instruction count
float cloudFBM(float2 p)
{
    float2 q = float2(fbm(p + float2(0.0f, 0.0f)),
                      fbm(p + float2(5.2f, 1.3f)));
    return fbm(p + 3.5f * q);
}

// ─────────────────────────────────────────────────────────────────────────────

PS_Out main(PS_In IN)
{
    float4 viewRay = mul(u_InvProjection, float4(IN.NDC, 1.0f, 1.0f));
    viewRay.xyz /= viewRay.w;
    float3 worldRay = normalize(mul((float3x3)u_InvView, viewRay.xyz));

    float3 sunDir    = normalize(u_SunDir);
    float  sunElev   = sunDir.y;                      // +1 = zenith, 0 = horizon, -1 = below
    float  cosSun    = dot(worldRay, sunDir);

    // ── Preetham sky ──────────────────────────────────────────────────────────
    float3 sky;
    if (worldRay.y >= 0.0f)
    {
        sky = PreethamSky(worldRay, sunDir, u_Turbidity) * u_SkyExposure;
    }
    else
    {
        float3 horizonSky = PreethamSky(normalize(float3(worldRay.x, 0.001f, worldRay.z)),
                                        sunDir, u_Turbidity) * u_SkyExposure;
        float t = smoothstep(0.0f, -0.08f, worldRay.y);
        // Ground colour is a direct linear value, not kcd/m² — don't scale by SkyExposure
        sky = lerp(horizonSky, u_GroundColor, t);
    }

    // ── Night sky blend ───────────────────────────────────────────────────────
    // When the sun is near/below the horizon the Preetham model clamps thetaSun
    // to 90° and produces very low output.  Blend in a static night-sky colour.
    {
        float dawnFrac  = saturate(sunElev * 6.0f + 0.3f);  // 0 when sun ≤ –5°, 1 when sun ≥ +12°
        float heightFac = 0.6f + worldRay.y * 0.4f;
        float3 nightColor = float3(0.007f, 0.010f, 0.035f) * heightFac;
        sky = lerp(nightColor, sky, dawnFrac);
    }

    // ── Sun disk with limb darkening (NOT scaled by SkyExposure) ─────────────
    // The sun disk brightness is controlled solely by u_SunStrength so it stays
    // bloom-worthy regardless of SkyExposure.  Fade out as sun dips below horizon.
    {
        float sunEdge   = 0.0005f;
        float sunMask   = smoothstep(u_SunSize - sunEdge, u_SunSize + sunEdge, cosSun);
        float diskFrac  = saturate((cosSun - u_SunSize) / max(1.0f - u_SunSize, 1e-5f));
        float limbDark  = 0.4f + 0.6f * sqrt(diskFrac);
        float sunAbove  = saturate(sunElev * 10.0f + 0.5f); // fades out as sun sets
        sky += u_SunColor * (sunMask * limbDark * u_SunStrength * sunAbove);
    }

    // ── Procedural cloud layer ────────────────────────────────────────────────
    if (u_CloudDensity > 0.0f && worldRay.y > 0.005f)
    {
        float3 camPos   = u_InvView[3].xyz;
        float  tHit     = (u_CloudHeight - camPos.y) / max(worldRay.y, 0.0001f);

        if (tHit > 0.0f)
        {
            float3 hitPos   = camPos + worldRay * tHit;
            float2 uv       = hitPos.xz / max(u_CloudScale, 1.0f);
            float2 scrolled = uv + float2(u_Time * u_CloudScrollSpeed,
                                          u_Time * u_CloudScrollSpeed * 0.7f);

            float noise     = cloudFBM(scrolled);
            float threshold = 1.0f - u_CloudCoverage;
            float softWidth = max(0.02f, u_CloudCoverage * 0.20f);
            float rawCloud  = saturate((noise - threshold) / softWidth);

            // Self-shadow: offset sample toward the projected sun direction
            float2 sunXZ     = float2(sunDir.x, sunDir.z);
            float  sunYSafe  = max(sunDir.y, 0.05f);
            float  shadowNoise = cloudFBM(scrolled + (sunXZ / sunYSafe) * 0.05f);
            float  selfShadow  = 1.0f - saturate(
                saturate((shadowNoise - threshold) / softWidth) * 0.8f);

            float cloudAlpha = rawCloud * u_CloudDensity;

            // Two-stage fade: angle fade to hide grazing geometry + distance fade
            // to prevent the flat cloud plane from being visible at the horizon.
            cloudAlpha *= smoothstep(0.0f, 0.22f, worldRay.y);
            cloudAlpha *= saturate(5000.0f / max(tHit, 1.0f));

            if (cloudAlpha > 0.001f)
            {
                float sunElevSat = saturate(sunElev);

                // Lit top: neutral white.  Shadow underside: cool blue-grey.
                float3 kLit   = u_CloudColor;
                float3 kShade = u_CloudColor * float3(0.28f, 0.32f, 0.48f);

                float  viewUp   = saturate(worldRay.y * 5.0f);
                float3 cloudCol = lerp(kShade, kLit,
                                       saturate(viewUp + selfShadow * 0.55f));
                cloudCol *= lerp(0.08f, 1.0f, sunElevSat); // dim at night/dusk

                // Warm tint only near the horizon (< ~17° elevation).
                // sunsetFrac = 0 when sun is above 1/6 ≈ 17°.
                float sunsetFrac = saturate(1.0f - sunElevSat * 6.0f);
                float rimStr     = saturate(cosSun * 0.5f + 0.5f) * sunsetFrac;
                cloudCol = lerp(cloudCol,
                                u_CloudColor * float3(1.0f, 0.52f, 0.16f),
                                rimStr * 0.65f);

                // Subtle silver lining at cloud edges toward sun
                float cloudEdge = (1.0f - rawCloud) * rawCloud * 4.0f;
                cloudCol += u_SunColor * cloudEdge
                          * saturate(cosSun) * sunElevSat * 0.18f;

                sky = lerp(sky, cloudCol, saturate(cloudAlpha));
            }
        }
    }

    PS_Out OUT;
    OUT.Color    = float4(sky, 1.0f);
    OUT.EntityID = -1;
    return OUT;
}
