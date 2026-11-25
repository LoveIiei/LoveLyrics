// SPDX-License-Identifier: AGPL-3.0-or-later
// Film grain + vignette over the ambient background. The noise dithers away the banding
// that a heavily blurred, upscaled cover would otherwise show.
#version 440

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float time;
    float grain;     // 0..1
    float vignette;  // 0..1
    vec2 resolution;
};

float hash(vec2 p)
{
    p = fract(p * vec2(443.897, 441.423));
    p += dot(p, p.yx + 19.19);
    return fract((p.x + p.y) * p.x);
}

void main()
{
    vec2 px = floor(qt_TexCoord0 * resolution);
    float n = hash(px + fract(time) * 97.0) - 0.5;

    vec2 c = qt_TexCoord0 - 0.5;
    c.x *= resolution.x / max(resolution.y, 1.0);
    float v = smoothstep(0.35, 1.15, length(c)) * vignette;

    // Premultiplied: darken towards the edges, then add signed noise.
    float a = clamp(v + abs(n) * grain * 0.12, 0.0, 1.0);
    vec3 rgb = vec3(max(n, 0.0) * grain * 0.12);
    fragColor = vec4(rgb, a) * qt_Opacity;
}
