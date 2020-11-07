// #version 450 core

// layout(location = 0) in vec3 Position;
// layout(location = 1) in vec3 inColor;

// layout(location = 0) out vec4 Color;


// void main ()
// {
// 	gl_Position = vec4(Position.xyz, 1.0f);
//     Color = vec4(inColor, 1.0f);
// }

/* Write your header comments here */
#version 450 core

precision highp float;
precision highp int; 

layout(location = 0) in vec3 POSITION;
layout(location = 1) in vec3 COLOR;
layout(location = 0) out vec4 vertOutput_COLOR;

struct VSInput
{
    vec3 Position;
    vec3 Color;
};
struct VSOutput
{
    vec4 Position;
    vec4 Color;
};
VSOutput HLSLmain(VSInput input1)
{
    VSOutput result;
    ((result).Position = vec4((input1).Position, 1.0));
    ((result).Color = vec4((input1).Color, 1.0));
    return result;
}
void main()
{
    VSInput input1;
    input1.Position = POSITION;
    input1.Color = COLOR;
    VSOutput result = HLSLmain(input1);
    gl_Position = result.Position;
    vertOutput_COLOR = result.Color;
}

