struct RayPayload
{
	float4 color;
};

// The following structure is declared in HLSL to represent hit attributes for fixed-function triangle intersection:
// struct BuiltInTriangleIntersectionAttributes
// {
//     float2 barycentrics;
// };
struct IntersectionAttribs
{
	float2 baryCrd;
};

[shader("closesthit")]
void chs(inout RayPayload payload : SV_RayPayload, IntersectionAttribs attribs : SV_IntersectionAttributes)
{
	float3 barycentrics = float3(1 - attribs.baryCrd.x - attribs.baryCrd.y, attribs.baryCrd.x, attribs.baryCrd.y);
    payload.color = float4(barycentrics, 1);
}
