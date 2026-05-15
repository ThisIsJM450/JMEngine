#pragma once
#include <string>

class Texture
{
public:
    
};

struct TextureFileName
{
    // ~ Start StoneBricks
    static std::string GetTexture_StoneBricks_Albedo()      { return "D:\\Projects\\JMEngine\\Contents\\Texture\\StoneBricksSplitface001\\StoneBricksSplitface001_COL_2K.jpg"; }
    static std::string GetTexture_StoneBricks_Normal()      { return "D:\\Projects\\JMEngine\\Contents\\Texture\\StoneBricksSplitface001\\StoneBricksSplitface001_NRM_2K.png"; }
    static std::string GetTexture_StoneBricks_AO()          { return "D:\\Projects\\JMEngine\\Contents\\Texture\\StoneBricksSplitface001\\StoneBricksSplitface001_AO_2K.jpg"; }
    static std::string GetTexture_StoneBricks_Gloss()       { return "D:\\Projects\\JMEngine\\Contents\\Texture\\StoneBricksSplitface001\\StoneBricksSplitface001_GLOSS_2K.jpg"; } // roughness = 1 - gloss
    static std::string GetTexture_StoneBricks_Displacement() { return "D:\\Projects\\JMEngine\\Contents\\Texture\\StoneBricksSplitface001\\StoneBricksSplitface001_DISP16_2K.tiff"; }
    static std::string GetTexture_StoneBricks_Bump()        { return "D:\\Projects\\JMEngine\\Contents\\Texture\\StoneBricksSplitface001\\StoneBricksSplitface001_BUMP16_2K.tiff"; }
    static std::string GetTexture_StoneBricks_Reflection()  { return "D:\\Projects\\JMEngine\\Contents\\Texture\\StoneBricksSplitface001\\StoneBricksSplitface001_REFL_2K.jpg"; } // 보통 MR 파이프라인에선 미사용
    // ~ End StoneBricks
    
    // ~ Start MetalGoldPaint
    static std::string GetTexture_MetalGoldPaint_Albedo()        { return "D:\\Projects\\JMEngine\\Contents\\Texture\\Poligon_MetalGoldPaint\\Poliigon_MetalGoldPaint_7253_BaseColor.jpg"; }
    static std::string GetTexture_MetalGoldPaint_Normal()        { return "D:\\Projects\\JMEngine\\Contents\\Texture\\Poligon_MetalGoldPaint\\Poliigon_MetalGoldPaint_7253_Normal.png"; }
    static std::string GetTexture_MetalGoldPaint_Metallic()      { return "D:\\Projects\\JMEngine\\Contents\\Texture\\Poligon_MetalGoldPaint\\Poliigon_MetalGoldPaint_7253_Metallic.jpg"; }
    static std::string GetTexture_MetalGoldPaint_Roughness()     { return "D:\\Projects\\JMEngine\\Contents\\Texture\\Poligon_MetalGoldPaint\\Poliigon_MetalGoldPaint_7253_Roughness.jpg"; }
    static std::string GetTexture_MetalGoldPaint_AO()            { return "D:\\Projects\\JMEngine\\Contents\\Texture\\Poligon_MetalGoldPaint\\Poliigon_MetalGoldPaint_7253_AmbientOcclusion.jpg"; }
    static std::string GetTexture_MetalGoldPaint_Displacement()  { return "D:\\Projects\\JMEngine\\Contents\\Texture\\Poligon_MetalGoldPaint\\Poliigon_MetalGoldPaint_7253_Displacement.tiff"; }
    static std::string GetTexture_MetalGoldPaint_MTLX()          { return "D:\\Projects\\JMEngine\\Contents\\Texture\\Poligon_MetalGoldPaint\\Poliigon_MetalGoldPaint_7253.mtlx"; } // 머티리얼 정의 파일(텍스처 아님)
    // ~ End MetalGoldPaint
    
    // ~ Start GrassPatchyGround
    static std::string GetTexture_GrassPatchyGround_Albedo()       { return "D:\\Projects\\JMEngine\\Contents\\Texture\\GrassPatchyGround\\Poliigon_GrassPatchyGround_4585_BaseColor.jpg"; }
    static std::string GetTexture_GrassPatchyGround_Normal()       { return "D:\\Projects\\JMEngine\\Contents\\Texture\\GrassPatchyGround\\Poliigon_GrassPatchyGround_4585_Normal.png"; }
    static std::string GetTexture_GrassPatchyGround_AO()           { return "D:\\Projects\\JMEngine\\Contents\\Texture\\GrassPatchyGround\\Poliigon_GrassPatchyGround_4585_AmbientOcclusion.jpg"; }
    static std::string GetTexture_GrassPatchyGround_Roughness()    { return "D:\\Projects\\JMEngine\\Contents\\Texture\\GrassPatchyGround\\Poliigon_GrassPatchyGround_4585_Roughness.jpg"; }
    static std::string GetTexture_GrassPatchyGround_Metallic()     { return "D:\\Projects\\JMEngine\\Contents\\Texture\\GrassPatchyGround\\Poliigon_GrassPatchyGround_4585_Metallic.jpg"; }
    static std::string GetTexture_GrassPatchyGround_Displacement() { return "D:\\Projects\\JMEngine\\Contents\\Texture\\GrassPatchyGround\\Poliigon_GrassPatchyGround_4585_Displacement.tiff"; }
    // ~ End GrassPatchyGround
    
    static std::string GetTexture_Wall()  { return "D:\\Projects\\JMEngine\\Contents\\Texture\\wall.jpg";}
    static std::string GetTexture_White()  { return "D:\\Projects\\JMEngine\\Contents\\Texture\\WhiteTexture.png";}
    
    // ~ Start Environment 
    static std::string GetTexture_CubeMap()  { return "D:\\Projects\\JMEngine\\Contents\\Texture\\DayEnvironmentHDRI057_1K_HDR.exr";}
    static std::string GetTexture_Environment()  { return "D:\\Projects\\JMEngine\\Contents\\Texture\\HDRI\\OutdoorEnvHDR.dds";}
    static std::string GetTexture_Environment_Diffuse()  { return "D:\\Projects\\JMEngine\\Contents\\Texture\\HDRI\\OutdoorDiffuseHDR.dds";}
    static std::string GetTexture_Environment_PrefilterMap()  { return "D:\\Projects\\JMEngine\\Contents\\Texture\\HDRI\\OutdoorSpecularHDR.dds";}
    static std::string GetTexture_Environment_LookUpTexture()  { return "D:\\Projects\\JMEngine\\Contents\\Texture\\HDRI\\OutdoorBrdf.dds";}
    
    static std::string GetTexture_Environment_Day()  { return "D:\\Projects\\JMEngine\\Contents\\Texture\\HDRI\\DaySky\\DaySkyHDRIEnvHDR.dds";}
    static std::string GetTexture_Environment_Day_Diffuse()  { return "D:\\Projects\\JMEngine\\Contents\\Texture\\HDRI\\DaySky\\DaySkyHDRIDiffuseHDR.dds";}
    static std::string GetTexture_Environment_Day_PrefilterMap()  { return "D:\\Projects\\JMEngine\\Contents\\Texture\\HDRI\\DaySky\\DaySkyHDRISpecularHDR.dds";}
    static std::string GetTexture_Environment_Day_LookUpTexture()  { return "D:\\Projects\\JMEngine\\Contents\\Texture\\HDRI\\DaySky\\DaySkyHDRIBrdf.dds";}
     
    
    static std::string GetTexture_Environment_Moonless()  { return "D:\\Projects\\JMEngine\\Contents\\Texture\\HDRI\\Moonless\\MoonlessEnvHDR.dds";}
    static std::string GetTexture_Environment_Moonless_Diffuse()  { return "D:\\Projects\\JMEngine\\Contents\\Texture\\HDRI\\Moonless\\MoonlessDiffuseHDR.dds";}
    static std::string GetTexture_Environment_Moonless_PrefilterMap()  { return "D:\\Projects\\JMEngine\\Contents\\Texture\\HDRI\\Moonless\\MoonlessSpecularHDR.dds";}
    static std::string GetTexture_Environment_Moonless_LookUpTexture()  { return "D:\\Projects\\JMEngine\\Contents\\Texture\\HDRI\\Moonless\\MoonlessBrdf.dds";}
    // ~ End Environment
};