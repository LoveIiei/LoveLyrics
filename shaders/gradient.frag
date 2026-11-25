// SPDX-License-Identifier: AGPL-3.0-or-later
// Slowly moving "mesh" gradient from four palette colours. Used when there is no cover,
// for the idle screen, and as the optional background mode.
#version 440

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float time;
    vec2 resolution;
    vec4 color1;
    vec4 color2;
    vec4 color3;
    vec4 color4;
};

float blob(vec2 uv, vec2 center, float radius)
{
    float d = length(uv - center);
    return exp(-(d * d) / (radius * radius));
}

void main()
{
    vec2 uv = qt_TexCoord0;
    uv.x *= resolution.x / max(resolution.y, 1.0);
    float aspect = resolution.x / max(resolution.y, 1.0);
    float t = time;

    vec2 p1 = vec2(aspect * (0.25 + 0.15 * sin(t * 0.13)), 0.30 + 0.20 * cos(t * 0.11));
    vec2 p2 = vec2(aspect * (0.75 + 0.15 * cos(t * 0.09)), 0.25 + 0.15 * sin(t * 0.15));
    vec2 p3 = vec2(aspect * (0.60 + 0.20 * sin(t * 0.07 + 1.0)), 0.80 + 0.12 * cos(t * 0.10));
    vec2 p4 = vec2(aspect * (0.20 + 0.10 * cos(t * 0.12 + 2.0)), 0.75 + 0.15 * sin(t * 0.08));

    float w1 = blob(uv, p1, 0.55);
    float w2 = blob(uv, p2, 0.50);
    float w3 = blob(uv, p3, 0.60);
    float w4 = blob(uv, p4, 0.45);
    float sum = w1 + w2 + w3 + w4 + 0.35;

    vec3 col = (color1.rgb * w1 + color2.rgb * w2 + color3.rgb * w3 + color4.rgb * w4
                + color1.rgb * 0.35) / sum;
    fragColor = vec4(col, 1.0) * qt_Opacity;
}
