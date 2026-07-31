#include "FortnitePorting/Public/Processing/MaterialMappings.h"

// -----------------------------------------------------------------------------------
// Default material slot mappings. Maps Fortnite material parameter names (as they
// come from the asset exports) to the corresponding parameter names exposed by
// /FortnitePorting/Materials/M_FP_Default.
//
// This table has been significantly expanded from the original to cover many more
// commonly encountered Fortnite parameter names so materials look correct on first
// import without manual tweaking.
// -----------------------------------------------------------------------------------
const FMappingCollection FMaterialMappings::Default {
	.Textures = {
		// BaseColor / Diffuse
		FSlotMapping("Diffuse"),
		FSlotMapping("D", "Diffuse"),
		FSlotMapping("Base Color", "Diffuse"),
		FSlotMapping("BaseColor", "Diffuse"),
		FSlotMapping("BaseColorMap", "Diffuse"),
		FSlotMapping("Albedo", "Diffuse"),
		FSlotMapping("AlbedoMap", "Diffuse"),
		FSlotMapping("DiffuseMap", "Diffuse"),
		FSlotMapping("DiffuseTexture", "Diffuse"),
		FSlotMapping("Color", "Diffuse"),
		FSlotMapping("ColorMap", "Diffuse"),
		FSlotMapping("PrimaryColor", "Diffuse"),
		FSlotMapping("Concrete", "Diffuse"),
		FSlotMapping("Trunk_BaseColor", "Diffuse"),
		FSlotMapping("Diffuse Top", "Diffuse"),
		FSlotMapping("BaseColor_Trunk", "Diffuse"),
		FSlotMapping("CliffTexture", "Diffuse"),
		FSlotMapping("PM_Diffuse", "Diffuse"),
		FSlotMapping("Tex", "Diffuse"),
		FSlotMapping("Texture", "Diffuse"),
		FSlotMapping("T_Diffuse", "Diffuse"),
		FSlotMapping("DayTexture", "Diffuse"),

		// Layered / background materials
		FSlotMapping("Background Diffuse"),
		FSlotMapping("BG Diffuse Texture", "Background Diffuse"),
		FSlotMapping("BackdropDiffuse", "Background Diffuse"),
		FSlotMapping("Background Diffuse Texture", "Background Diffuse"),

		// M / Mask
		FSlotMapping("M"),
		FSlotMapping("Mask", "M"),
		FSlotMapping("MaskTexture", "M"),
		FSlotMapping("OpacityMask", "MaskTexture"),
		FSlotMapping("OpacityMaskTexture", "MaskTexture"),
		FSlotMapping("Opacity", "MaskTexture"),
		FSlotMapping("Alpha", "MaskTexture"),
		FSlotMapping("AlphaTexture", "MaskTexture"),
		FSlotMapping("OpacityMap", "MaskTexture"),
		FSlotMapping("EmissiveMask", "M"),

		// Specular / Packed Masks (MRO / MRS / SRM / ORM packing)
		FSlotMapping("SpecularMasks"),
		FSlotMapping("S", "SpecularMasks"),
		FSlotMapping("SRM", "SpecularMasks", "SwizzleRoughnessToGreen"),
		FSlotMapping("MRS", "SpecularMasks"),
		FSlotMapping("MRO", "SpecularMasks"),
		FSlotMapping("ORM", "SpecularMasks"),
		FSlotMapping("RMA", "SpecularMasks"),
		FSlotMapping("ORMTexture", "SpecularMasks"),
		FSlotMapping("PackedTexture", "SpecularMasks"),
		FSlotMapping("Specular Mask", "SpecularMasks"),
		FSlotMapping("SpecularMask", "SpecularMasks"),
		FSlotMapping("SpecularMap", "SpecularMasks"),
		FSlotMapping("Specular", "SpecularMasks"),
		FSlotMapping("Concrete_SpecMask", "SpecularMasks"),
		FSlotMapping("Trunk_Specular", "SpecularMasks"),
		FSlotMapping("Specular Top", "SpecularMasks"),
		FSlotMapping("SMR_Trunk", "SpecularMasks"),
		FSlotMapping("Cliff Spec Texture", "SpecularMasks"),
		FSlotMapping("Masks", "SpecularMasks"),
		FSlotMapping("MaskMap", "SpecularMasks"),
		FSlotMapping("PackedMasks", "SpecularMasks"),

		// Metallic (separate map)
		FSlotMapping("Metallic", "SpecularMasks"),
		FSlotMapping("MetallicMap", "SpecularMasks"),
		FSlotMapping("MetallicTexture", "SpecularMasks"),

		// Roughness (separate map)
		FSlotMapping("Roughness", "SpecularMasks"),
		FSlotMapping("RoughnessMap", "SpecularMasks"),
		FSlotMapping("RoughnessTexture", "SpecularMasks"),

		// Ambient Occlusion (separate map)
		FSlotMapping("AO", "SpecularMasks"),
		FSlotMapping("AmbientOcclusion", "SpecularMasks"),
		FSlotMapping("AmbientOcclusionMap", "SpecularMasks"),
		FSlotMapping("AOMap", "SpecularMasks"),
		FSlotMapping("Occlusion", "SpecularMasks"),
		FSlotMapping("OcclusionMap", "SpecularMasks"),

		// Normal
		FSlotMapping("Normals"),
		FSlotMapping("N", "Normals"),
		FSlotMapping("Normal", "Normals"),
		FSlotMapping("NormalMap", "Normals"),
		FSlotMapping("NormalTexture", "Normals"),
		FSlotMapping("NormalsMap", "Normals"),
		FSlotMapping("ConcreteTextureNormal", "Normals"),
		FSlotMapping("Trunk_Normal", "Normals"),
		FSlotMapping("Normals Top", "Normals"),
		FSlotMapping("Normal_Trunk", "Normals"),
		FSlotMapping("CliffNormal", "Normals"),
		FSlotMapping("PM_Normals", "Normals"),
		FSlotMapping("T_Normals", "Normals"),
		FSlotMapping("BumpMap", "Normals"),
		FSlotMapping("Bump", "Normals"),
		FSlotMapping("NormalMapTexture", "Normals"),

		// Emissive
		FSlotMapping("EmissiveTexture", "EmissiveTexture"),
		FSlotMapping("Emissive", "EmissiveTexture"),
		FSlotMapping("EmissiveMap", "EmissiveTexture"),
		FSlotMapping("EmissiveColorMap", "EmissiveTexture"),
		FSlotMapping("GlowTexture", "EmissiveTexture"),
		FSlotMapping("Glow", "EmissiveTexture"),
		FSlotMapping("EmissiveMaskTexture", "EmissiveTexture"),
		FSlotMapping("Lights", "EmissiveTexture"),
		FSlotMapping("LightTexture", "EmissiveTexture"),
		FSlotMapping("NeonTexture", "EmissiveTexture"),
		FSlotMapping("SelfIllum", "EmissiveTexture"),
		FSlotMapping("SelfIllumination", "EmissiveTexture"),

		// Detail / weathering
		FSlotMapping("DetailNormal", "Normals"),
		FSlotMapping("DetailNormalMap", "Normals"),
		FSlotMapping("DetailDiffuse", "Diffuse"),
		FSlotMapping("DetailTexture", "Diffuse"),
		FSlotMapping("DetailMasks", "SpecularMasks"),
		FSlotMapping("WeatherTexture", "SpecularMasks"),
		FSlotMapping("DirtTexture", "Diffuse"),
		FSlotMapping("GrungeTexture", "SpecularMasks"),

		// Niagara/particle specific
		FSlotMapping("SubImage", "Diffuse"),
		FSlotMapping("SubUVTexture", "Diffuse"),
		FSlotMapping("SubUV", "Diffuse"),
		FSlotMapping("ParticleColor", "Diffuse"),
		FSlotMapping("Flipbook", "Diffuse"),
		FSlotMapping("SpriteTexture", "Diffuse"),
	},

	.Scalars = {
		FSlotMapping("RoughnessMin", "Roughness Min"),
		FSlotMapping("SpecRoughnessMin", "Roughness Min"),
		FSlotMapping("RawRoughnessMin", "Roughness Min"),
		FSlotMapping("Rough Min", "Roughness Min"),
		FSlotMapping("MinRoughness", "Roughness Min"),

		FSlotMapping("RoughnessMax", "Roughness Max"),
		FSlotMapping("SpecRoughnessMax", "Roughness Max"),
		FSlotMapping("RawRoughnessMax", "Roughness Max"),
		FSlotMapping("Rough Max", "Roughness Max"),
		FSlotMapping("MaxRoughness", "Roughness Max"),

		FSlotMapping("MetallicScalar", "Metallic"),
		FSlotMapping("Metal", "Metallic"),
		FSlotMapping("Metallic", "Metallic"),

		FSlotMapping("RoughnessScalar", "Roughness"),
		FSlotMapping("RoughnessConstant", "Roughness"),
		FSlotMapping("Rough", "Roughness"),

		FSlotMapping("EmissiveStrength", "Emissive"),
		FSlotMapping("EmissiveIntensity", "Emissive"),
		FSlotMapping("EmissiveMultiplier", "Emissive"),
		FSlotMapping("GlowIntensity", "Emissive"),
		FSlotMapping("EmissiveBoost", "Emissive"),

		FSlotMapping("OpacityValue", "Opacity"),
		FSlotMapping("OpacityConstant", "Opacity"),
		FSlotMapping("OpacityMaskValue", "OpacityMaskClip"),
		FSlotMapping("OpacityMaskClipValue", "OpacityMaskClip"),
		FSlotMapping("OpacityMaskClip", "OpacityMaskClip"),
		FSlotMapping("Refraction", "Refraction"),
		FSlotMapping("RefractionDepthBias", "Refraction"),
		FSlotMapping("IOR", "Refraction"),
	},

	.Switches = {
		FSlotMapping("SwizzleRoughnessToGreen"),
		FSlotMapping("Use 2 Layers"),
		FSlotMapping("Use 3 Layers"),
		FSlotMapping("Use Emissive"),
		FSlotMapping("UseEmissive", "Use Emissive"),
		FSlotMapping("bUseEmissive", "Use Emissive"),
		FSlotMapping("TwoSided"),
		FSlotMapping("bTwoSided", "TwoSided"),
	}
};

// -----------------------------------------------------------------------------------
// Layered material mappings - supports up to 6 stacked material layers for
// buildings/landscapes. Layers beyond 6 are folded down onto layer 6.
// -----------------------------------------------------------------------------------
const FMappingCollection FMaterialMappings::Layer {
	.Textures = {
		FSlotMapping("Diffuse"),
		FSlotMapping("SpecularMasks"),
	    FSlotMapping("Normals"),
	    FSlotMapping("EmissiveTexture"),
	    FSlotMapping("MaskTexture"),
	    FSlotMapping("Background Diffuse"),

	    FSlotMapping("Diffuse_Texture_2"),
	    FSlotMapping("SpecularMasks_2"),
	    FSlotMapping("Normals_Texture_2"),
	    FSlotMapping("Emissive_Texture_2"),
	    FSlotMapping("MaskTexture_2"),
	    FSlotMapping("Background Diffuse 2"),

	    FSlotMapping("Diffuse_Texture_3"),
	    FSlotMapping("SpecularMasks_3"),
	    FSlotMapping("Normals_Texture_3"),
	    FSlotMapping("Emissive_Texture_3"),
	    FSlotMapping("MaskTexture_3"),
	    FSlotMapping("Background Diffuse 3"),

	    FSlotMapping("Diffuse_Texture_4"),
	    FSlotMapping("SpecularMasks_4"),
	    FSlotMapping("Normals_Texture_4"),
	    FSlotMapping("Emissive_Texture_4"),
	    FSlotMapping("MaskTexture_4"),
	    FSlotMapping("Background Diffuse 4"),

	    FSlotMapping("Diffuse_Texture_5"),
	    FSlotMapping("SpecularMasks_5"),
	    FSlotMapping("Normals_Texture_5"),
	    FSlotMapping("Emissive_Texture_5"),
	    FSlotMapping("MaskTexture_5"),
	    FSlotMapping("Background Diffuse 5"),

	    FSlotMapping("Diffuse_Texture_6"),
	    FSlotMapping("SpecularMasks_6"),
	    FSlotMapping("Normals_Texture_6"),
	    FSlotMapping("Emissive_Texture_6"),
	    FSlotMapping("MaskTexture_6"),
	    FSlotMapping("Background Diffuse 6"),
	},
};
