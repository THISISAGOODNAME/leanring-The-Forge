cbuffer uniformBlock : register(b0, UPDATE_FREQ_PER_FRAME)
{
    int prog;
    float mixWeight;
};

struct VSInput
{
    float3 Position : POSITION;
    float3 Color : COLOR;
    float2 UV : TEXCOORD0;
};

struct VSOutput {
	float4 Position : SV_POSITION;
    float4 Color : COLOR;
    float2 UV : TEXCOORD0;
};

VSOutput main(VSInput input)
{
    VSOutput result;
    result.Position = float4(input.Position, 1.0);
    result.Color = float4(input.Color, 1.0);
    result.UV = input.UV;
    return result;
}
