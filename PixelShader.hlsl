struct PS_INPUT
{
    float4 inPosition : SV_Position;
    float2 inTexCoord : TEXCOORD;
};

Texture2D objTexture : TEXTURE : register(t0);          //t0 - use first register slot for texture
SamplerState objSamplerState : SAMPLER : register(s0); //s0 - use first register slot for sampler

float4 main(PS_INPUT input) : SV_TARGET
{
    float3 pixelColor = objTexture.Sample(objSamplerState, input.inTexCoord);
	return float4(pixelColor, 1.0f);
}