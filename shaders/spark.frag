// SPDX-License-Identifier: AGPL-3.0-or-later
// SparkField: soft glow dots with a hot core, and trail segments that fade towards their
// tail. Output is premultiplied with alpha 0, so normal premultiplied blending adds the
// light (fireworks are additive).
#version 440

layout(location = 0) in vec3 uv;
layout(location = 1) in vec4 color;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
};

void main()
{
    float a;
    if (uv.z < 0.5) {
        // Dot: wide glow + bright core, whitened in the middle.
        float d2 = dot(uv.xy, uv.xy);
        float glow = exp(-d2 * 3.2);
        float core = exp(-d2 * 26.0);
        a = glow * 0.75 + core;
        vec3 rgb = mix(color.rgb, vec3(1.0), core * 0.55);
        fragColor = vec4(rgb * a * color.a, 0.0);
        return;
    }
    // Trail: x = 0 at the tail .. 1 at the head, y = -1..1 across.
    float across = exp(-uv.y * uv.y * 3.0);
    float along = uv.x * uv.x;
    a = across * along;
    fragColor = vec4(color.rgb * a * color.a, 0.0);
}
