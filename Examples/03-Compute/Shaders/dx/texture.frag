cbuffer uniformBlock : register(b0, UPDATE_FREQ_PER_FRAME)
{
    int prog;
    float mixWeight;
}

struct VSOutput {
	float4 Position : SV_POSITION;
    float4 Color : COLOR;
    float2 UV : TEXCOORD0;
};

Texture2D Tex0 : register(t1);
Texture2D Tex1 : register(t2);
SamplerState uSampler0 : register(s3);

float4 main(VSOutput input) : SV_TARGET
{
    float2 tUV = input.UV;

    if (prog == 0)
    {
        return input.Color;
    }
    else if (prog == 1)
    {
        return Tex0.Sample(uSampler0, tUV);
    }
    else if (prog == 2)
    {
        return Tex1.Sample(uSampler0, tUV);
    }
    else
    {
        return lerp(Tex0.Sample(uSampler0, tUV), Tex1.Sample(uSampler0, tUV), mixWeight);
    }
}