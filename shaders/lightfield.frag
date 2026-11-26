// SPDX-License-Identifier: AGPL-3.0-or-later
// Lumiere light field: up to six volumetric beams, warped haze (beams only show where they
// pass through it), dust specks lit only inside the beams, and a glare at the light source,
// composited over a dark stage.
//
// Beam mask, haze and tone mapping follow Folia's Lumiere (AGPL-3.0,
// chthollyphile/folia-major), src/components/visualizer/lumiere/light/lightFieldShader.ts
// and light/rig.ts; qml/styles/Lumiere/lumiere.js computes the same beam mask on the CPU,
// so lyrics light up exactly where the beams are. Coordinates: height units, y down.
#version 440

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float time;
    float aspect;
    float beamCount;
    float octaves;
    vec4 fogA;        // density, tyndallBase, scale, warp
    vec4 fogB;        // driftX, driftY, ambient, motes
    vec4 glare;       // x, y (height units), radius, intensity
    vec4 glareB;      // streak
    vec4 fogColor;
    vec4 darkColor;   // premultiplied by its own alpha below
    // beams: A = origin.xy, dir.xy; B = halfWidth, tanSpread, softness, length;
    //        C = intensity, streaks, streakFreq, streakPhase; D = rgb, core
    vec4 b0A; vec4 b0B; vec4 b0C; vec4 b0D;
    vec4 b1A; vec4 b1B; vec4 b1C; vec4 b1D;
    vec4 b2A; vec4 b2B; vec4 b2C; vec4 b2D;
    vec4 b3A; vec4 b3B; vec4 b3C; vec4 b3D;
    vec4 b4A; vec4 b4B; vec4 b4C; vec4 b4D;
    vec4 b5A; vec4 b5B; vec4 b5C; vec4 b5D;
};

// Dave Hoskins' hashes (no sin), as in the original.
float hash11(float p)
{
    p = fract(p * 0.1031);
    p *= p + 33.33;
    p *= p + p;
    return fract(p);
}
float hash12(vec2 p)
{
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}
float valueNoise1(float x)
{
    float i = floor(x);
    float f = x - i;
    float u = f * f * (3.0 - 2.0 * f);
    return mix(hash11(i), hash11(i + 1.0), u);
}
float valueNoise2(vec2 p)
{
    vec2 i = floor(p);
    vec2 f = p - i;
    vec2 u = f * f * (3.0 - 2.0 * f);
    float a = hash12(i), b = hash12(i + vec2(1.0, 0.0));
    float c = hash12(i + vec2(0.0, 1.0)), d = hash12(i + vec2(1.0, 1.0));
    return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
}
float fbm(vec2 p, float oct)
{
    float sum = 0.0, amp = 0.5, norm = 0.0;
    mat2 rot = mat2(0.8, -0.6, 0.6, 0.8);
    for (int i = 0; i < 6; i++) {
        if (float(i) >= oct) break;
        sum += amp * valueNoise2(p);
        norm += amp;
        p = rot * p * 2.03 + vec2(17.1, 9.2);
        amp *= 0.5;
    }
    return sum / max(norm, 1e-4);
}
float haze(vec2 p, float t)
{
    vec2 q = (p - fogB.xy * t) * fogA.z;
    vec2 w = vec2(fbm(q + vec2(0.0, t * 0.07), 3.0), fbm(q + vec2(5.2, 1.3) - vec2(t * 0.05, 0.0), 3.0));
    float n = fbm(q + fogA.w * (w - 0.5) * 2.0, octaves);
    return smoothstep(0.28, 0.82, n); // wisps, not an even grey
}

float beamMask(vec4 A, vec4 B, vec4 C, vec4 D, vec2 p)
{
    vec2 d = p - A.xy;
    float along = dot(d, A.zw);
    if (along <= 0.0) return 0.0;
    float side = d.x * -A.w + d.y * A.z;
    float hw = abs(B.x + along * B.y) + 1e-4; // abs: converging beams open up past the focus
    float s = abs(side) / hw;
    float edge = smoothstep(1.0, 1.0 - max(B.z, 0.02), s);
    if (edge <= 0.0) return 0.0;
    float signedS = s * sign(side);
    float streak = 1.0 - C.y + C.y * valueNoise1(signedS * C.z + C.w);
    float core = 1.0 - D.w + D.w * (1.0 - s * s);
    float falloff = exp(-along / max(B.w, 0.001));
    float start = smoothstep(0.0, abs(B.x) * 2.0 + 0.01, along);
    return edge * streak * core * falloff * start * C.x;
}

// Dust: a jittered grid of tiny specks, two layers drifting at different speeds.
float dust(vec2 p, float t)
{
    float m = 0.0;
    for (int layer = 0; layer < 2; layer++) {
        float L = float(layer);
        vec2 q = p * (26.0 + L * 15.0) + vec2(t * 0.06 * (1.0 + L), -t * 0.1 * (1.0 + L));
        vec2 id = floor(q);
        vec2 f = q - id;
        float r1 = hash12(id), r2 = hash12(id + 19.7);
        vec2 c = vec2(0.2 + 0.6 * r1, 0.2 + 0.6 * r2)
               + 0.12 * vec2(sin(t * 0.7 + r1 * 6.3), cos(t * 0.5 + r2 * 6.3));
        float size = 0.045 + 0.055 * hash12(id + 3.1);
        float present = step(0.5, hash12(id + 7.7));
        float twinkle = 0.6 + 0.4 * sin(t * (1.0 + r1 * 2.0) + r2 * 6.3);
        m += smoothstep(size, 0.0, length(f - c)) * present * twinkle;
    }
    return m;
}

void main()
{
    vec2 p = vec2(qt_TexCoord0.x * aspect, qt_TexCoord0.y);
    float t = time;

    float density = haze(p, t);
    float tyndall = fogA.y + (1.0 - fogA.y) * density * fogA.x;

    vec3 beams = vec3(0.0);
    float sum = 0.0;
    float m;
    if (beamCount > 0.5) { m = beamMask(b0A, b0B, b0C, b0D, p); beams += b0D.rgb * m; sum += m; }
    if (beamCount > 1.5) { m = beamMask(b1A, b1B, b1C, b1D, p); beams += b1D.rgb * m; sum += m; }
    if (beamCount > 2.5) { m = beamMask(b2A, b2B, b2C, b2D, p); beams += b2D.rgb * m; sum += m; }
    if (beamCount > 3.5) { m = beamMask(b3A, b3B, b3C, b3D, p); beams += b3D.rgb * m; sum += m; }
    if (beamCount > 4.5) { m = beamMask(b4A, b4B, b4C, b4D, p); beams += b4D.rgb * m; sum += m; }
    if (beamCount > 5.5) { m = beamMask(b5A, b5B, b5C, b5D, p); beams += b5D.rgb * m; sum += m; }

    vec3 light = beams * tyndall;
    // Dust only shows inside the light, like specks in a projector beam.
    if (fogB.w > 0.0)
        light += beams * dust(p, t) * fogB.w * 1.6;
    // Faint haze outside the beams, for a sense of space.
    light += fogColor.rgb * density * fogB.z;

    if (glare.w > 0.0) {
        vec2 g = p - glare.xy;
        float r = max(glare.z, 1e-3);
        float dist = length(g);
        float glow = exp(-dist / r) * 0.55 + exp(-(dist * dist) / (r * r * 0.03));
        float streak = exp(-abs(g.y) / (r * 0.035)) * exp(-abs(g.x) / (r * 7.0)) * glareB.x;
        light += fogColor.rgb * 1.6 * (glow + streak) * glare.w;
    }

    // Hue-preserving compression (per channel would bleach gold to white).
    float peak = max(max(light.r, light.g), light.b);
    float mapped = 1.0 - exp(-peak);
    vec3 c = peak > 1e-5 ? light / peak * mapped : vec3(0.0);
    c = mix(c, vec3(mapped), 0.3 * mapped * mapped * mapped);
    c += (hash12(gl_FragCoord.xy) - 0.5) / 255.0; // dither
    c = clamp(c, 0.0, 1.0);
    float a = max(max(c.r, c.g), c.b);

    vec3 rgb = c + darkColor.rgb * darkColor.a * (1.0 - a);
    float alpha = a + darkColor.a * (1.0 - a);
    fragColor = vec4(rgb, alpha) * qt_Opacity;
}
