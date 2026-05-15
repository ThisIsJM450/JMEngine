// Param에서 8개 세팅 가능 (ParameterBlock::kMaxTextures)
// Texture2DArray로 처리할지 고민..
Texture2D ParamTexture[8] : register(t0); 

SamplerState CommonSampler : register(s0);
// Param에서 8개 세팅 가능 (ParameterBlock::kMaxSamplers)
SamplerState ParamSampler[8] : register(s3);


