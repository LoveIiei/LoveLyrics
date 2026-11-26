// SPDX-License-Identifier: AGPL-3.0-or-later
// SparkField: one quad per spark, trail segment or flash.
#version 440

layout(location = 0) in vec4 vertexCoord;
layout(location = 1) in vec3 vertexUv;    // uv in [-1, 1] (dot) or along/cross (trail); z = shape
layout(location = 2) in vec4 vertexColor; // straight (not premultiplied) rgba

layout(location = 0) out vec3 uv;
layout(location = 1) out vec4 color;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
};

void main()
{
    uv = vertexUv;
    color = vec4(vertexColor.rgb, vertexColor.a * qt_Opacity);
    gl_Position = qt_Matrix * vertexCoord;
}
