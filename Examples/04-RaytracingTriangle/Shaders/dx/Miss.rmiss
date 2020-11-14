struct RayPayload
{
	float4 color;
};

[shader("miss")]
void miss(inout RayPayload payload : SV_RayPayload)
{
	payload.color = float4(0.1f, 0.1f, 0.1f, 1.0f);
}
