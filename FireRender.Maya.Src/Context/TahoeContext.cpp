#include "TahoeContext.h"

rpr_int TahoeContext::m_gTahoePluginID = -1;

TahoeContext::TahoeContext()
{

}

rpr_int TahoeContext::GetPluginID()
{
	if (m_gTahoePluginID == -1)
	{
#ifdef OSMac_
		std::string pathToTahoeDll = "/Users/Shared/RadeonProRender/Maya/lib/libTahoe64.dylib";
		if (0 != access(pathToTahoeDll.c_str(), F_OK))
		{
			pathToTahoeDll = "/Users/Shared/RadeonProRender/lib/libTahoe64.dylib";
		}
		m_gTahoePluginID = rprRegisterPlugin(pathToTahoeDll.c_str());
#elif __linux__
		m_gTahoePluginID = rprRegisterPlugin("libTahoe64.so");
#else
		m_gTahoePluginID = rprRegisterPlugin("Tahoe64.dll");
#endif
	}

	return m_gTahoePluginID;
}

rpr_int TahoeContext::CreateContextInternal(rpr_creation_flags createFlags, rpr_context* pContext)
{
	rpr_int pluginID = GetPluginID();

	if (pluginID == -1)
	{
		MGlobal::displayError("Unable to register Radeon ProRender plug-in.");
		return -1;
	}

	rpr_int plugins[] = { pluginID };
	size_t pluginCount = 1;

	auto cachePath = getShaderCachePath();

	// setup CPU thread count
	std::vector<rpr_context_properties> ctxProperties;
#if (RPR_VERSION_MINOR < 34)
	ctxProperties.push_back((rpr_context_properties)RPR_CONTEXT_CREATEPROP_SAMPLER_TYPE);
#else
	ctxProperties.push_back((rpr_context_properties)RPR_CONTEXT_SAMPLER_TYPE);
#endif
	ctxProperties.push_back((rpr_context_properties)RPR_CONTEXT_SAMPLER_TYPE_CMJ);

	int threadCountToOverride = getThreadCountToOverride();
	if ((createFlags & RPR_CREATION_FLAGS_ENABLE_CPU) && threadCountToOverride > 0)
	{
#if (RPR_VERSION_MINOR < 34)
		ctxProperties.push_back((rpr_context_properties)RPR_CONTEXT_CREATEPROP_CPU_THREAD_LIMIT);
#else
		ctxProperties.push_back((rpr_context_properties)RPR_CONTEXT_CPU_THREAD_LIMIT);
#endif
		ctxProperties.push_back((void*)(size_t)threadCountToOverride);
	}

	ctxProperties.push_back((rpr_context_properties)0);

#ifdef RPR_VERSION_MAJOR_MINOR_REVISION
	int res = rprCreateContext(RPR_VERSION_MAJOR_MINOR_REVISION, plugins, pluginCount, createFlags, ctxProperties.data(), cachePath.asUTF8(), pContext);
#else
	int res = rprCreateContext(RPR_API_VERSION, plugins, pluginCount, createFlags, ctxProperties.data(), cachePath.asUTF8(), pContext);
#endif

	return res;
}

void TahoeContext::setupContext(const FireRenderGlobalsData& fireRenderGlobalsData, bool disableWhiteBalance)
{
	frw::Context context = GetContext();
	rpr_context frcontext = context.Handle();

	rpr_int frstatus = RPR_SUCCESS;

	frstatus = rprContextSetParameterByKey1f(frcontext, RPR_CONTEXT_PDF_THRESHOLD, 0.0000f);
	checkStatus(frstatus);

	if (GetRenderType() == RenderType::Thumbnail)
	{
		updateTonemapping(fireRenderGlobalsData, false);
		return;
	}

	frstatus = rprContextSetParameterByKey1f(frcontext, RPR_CONTEXT_RADIANCE_CLAMP, fireRenderGlobalsData.giClampIrradiance ? fireRenderGlobalsData.giClampIrradianceValue : FLT_MAX);
	checkStatus(frstatus);

	frstatus = rprContextSetParameterByKey1u(frcontext, RPR_CONTEXT_TEXTURE_COMPRESSION, fireRenderGlobalsData.textureCompression);
	checkStatus(frstatus);

	frstatus = rprContextSetParameterByKey1u(frcontext, RPR_CONTEXT_ADAPTIVE_SAMPLING_TILE_SIZE, fireRenderGlobalsData.adaptiveTileSize);
	checkStatus(frstatus);

	if (GetRenderType() == RenderType::ProductionRender) // production (final) rendering
	{
		frstatus = rprContextSetParameterByKey1f(frcontext, RPR_CONTEXT_ADAPTIVE_SAMPLING_THRESHOLD, fireRenderGlobalsData.adaptiveThreshold);
		checkStatus(frstatus);

		frstatus = rprContextSetParameterByKey1u(frcontext, RPR_CONTEXT_RENDER_MODE, fireRenderGlobalsData.renderMode);
		checkStatus(frstatus);

		setSamplesPerUpdate(fireRenderGlobalsData.samplesPerUpdate);

		frstatus = rprContextSetParameterByKey1u(frcontext, RPR_CONTEXT_MAX_RECURSION, fireRenderGlobalsData.maxRayDepth);
		checkStatus(frstatus);

		frstatus = rprContextSetParameterByKey1u(frcontext, RPR_CONTEXT_MAX_DEPTH_DIFFUSE, fireRenderGlobalsData.maxRayDepthDiffuse);
		checkStatus(frstatus);

		frstatus = rprContextSetParameterByKey1u(frcontext, RPR_CONTEXT_MAX_DEPTH_GLOSSY, fireRenderGlobalsData.maxRayDepthGlossy);
		checkStatus(frstatus);

		frstatus = rprContextSetParameterByKey1u(frcontext, RPR_CONTEXT_MAX_DEPTH_REFRACTION, fireRenderGlobalsData.maxRayDepthRefraction);
		checkStatus(frstatus);

		frstatus = rprContextSetParameterByKey1u(frcontext, RPR_CONTEXT_MAX_DEPTH_GLOSSY_REFRACTION, fireRenderGlobalsData.maxRayDepthGlossyRefraction);
		checkStatus(frstatus);

		frstatus = rprContextSetParameterByKey1u(frcontext, RPR_CONTEXT_MAX_DEPTH_SHADOW, fireRenderGlobalsData.maxRayDepthShadow);
		checkStatus(frstatus);

		frstatus = rprContextSetParameterByKey1u(frcontext, RPR_CONTEXT_ADAPTIVE_SAMPLING_MIN_SPP, fireRenderGlobalsData.completionCriteriaFinalRender.completionCriteriaMinIterations);
		checkStatus(frstatus);
	}
	else if (isInteractive())
	{
		setSamplesPerUpdate(1);

		frstatus = rprContextSetParameterByKey1f(frcontext, RPR_CONTEXT_ADAPTIVE_SAMPLING_THRESHOLD, fireRenderGlobalsData.adaptiveThresholdViewport);
		checkStatus(frstatus);

		frstatus = rprContextSetParameterByKey1u(frcontext, RPR_CONTEXT_RENDER_MODE, fireRenderGlobalsData.viewportRenderMode);
		checkStatus(frstatus);

		frstatus = rprContextSetParameterByKey1u(frcontext, RPR_CONTEXT_MAX_RECURSION, fireRenderGlobalsData.viewportMaxRayDepth);
		checkStatus(frstatus);

		frstatus = rprContextSetParameterByKey1u(frcontext, RPR_CONTEXT_MAX_DEPTH_DIFFUSE, fireRenderGlobalsData.viewportMaxDiffuseRayDepth);
		checkStatus(frstatus);

		frstatus = rprContextSetParameterByKey1u(frcontext, RPR_CONTEXT_MAX_DEPTH_GLOSSY, fireRenderGlobalsData.viewportMaxReflectionRayDepth);
		checkStatus(frstatus);

		frstatus = rprContextSetParameterByKey1u(frcontext, RPR_CONTEXT_MAX_DEPTH_REFRACTION, fireRenderGlobalsData.viewportMaxReflectionRayDepth);
		checkStatus(frstatus);

		frstatus = rprContextSetParameterByKey1u(frcontext, RPR_CONTEXT_MAX_DEPTH_GLOSSY_REFRACTION, fireRenderGlobalsData.viewportMaxReflectionRayDepth);
		checkStatus(frstatus);

		frstatus = rprContextSetParameterByKey1u(frcontext, RPR_CONTEXT_MAX_DEPTH_SHADOW, fireRenderGlobalsData.viewportMaxDiffuseRayDepth);
		checkStatus(frstatus);

		frstatus = rprContextSetParameterByKey1u(frcontext, RPR_CONTEXT_ADAPTIVE_SAMPLING_MIN_SPP, fireRenderGlobalsData.completionCriteriaViewport.completionCriteriaMinIterations);
		checkStatus(frstatus);
	}

	frstatus = rprContextSetParameterByKey1f(frcontext, RPR_CONTEXT_RAY_CAST_EPISLON, fireRenderGlobalsData.raycastEpsilon);
	checkStatus(frstatus);

	frstatus = rprContextSetParameterByKey1u(frcontext, RPR_CONTEXT_IMAGE_FILTER_TYPE, fireRenderGlobalsData.filterType);
	checkStatus(frstatus);

	//

	rpr_material_node_input filterAttrName = RPR_CONTEXT_IMAGE_FILTER_BOX_RADIUS;
	switch (fireRenderGlobalsData.filterType)
	{
	case 2:
	{
		filterAttrName = RPR_CONTEXT_IMAGE_FILTER_TRIANGLE_RADIUS;
		break;
	}
	case 3:
	{
		filterAttrName = RPR_CONTEXT_IMAGE_FILTER_GAUSSIAN_RADIUS;
		break;
	}
	case 4:
	{
		filterAttrName = RPR_CONTEXT_IMAGE_FILTER_MITCHELL_RADIUS;
		break;
	}
	case 5:
	{
		filterAttrName = RPR_CONTEXT_IMAGE_FILTER_LANCZOS_RADIUS;
		break;
	}
	case 6:
	{
		filterAttrName = RPR_CONTEXT_IMAGE_FILTER_BLACKMANHARRIS_RADIUS;
		break;
	}
	default:
		break;
	}

	frstatus = rprContextSetParameterByKey1f(frcontext, filterAttrName, fireRenderGlobalsData.filterSize);
	checkStatus(frstatus);

	frstatus = rprContextSetParameterByKey1u(frcontext, RPR_CONTEXT_METAL_PERFORMANCE_SHADER, fireRenderGlobalsData.useMPS ? 1 : 0);
	checkStatus(frstatus);

	updateTonemapping(fireRenderGlobalsData, disableWhiteBalance);
}

void TahoeContext::updateTonemapping(const FireRenderGlobalsData& fireRenderGlobalsData, bool disableWhiteBalance)
{
	frw::Context context = GetContext();
	rpr_context frcontext = context.Handle();

	rpr_int frstatus = RPR_SUCCESS;

	frstatus = rprContextSetParameterByKey1f(frcontext, RPR_CONTEXT_TEXTURE_GAMMA, fireRenderGlobalsData.textureGamma);
	checkStatus(frstatus);

	// Disable display gamma correction unless it is being applied
	// to Maya views. It will be always be enabled before file output.
	auto displayGammaValue = fireRenderGlobalsData.applyGammaToMayaViews ? fireRenderGlobalsData.displayGamma : 1.0f;
	frstatus = rprContextSetParameterByKey1f(frcontext, RPR_CONTEXT_DISPLAY_GAMMA, displayGammaValue);
	checkStatus(frstatus);

	// Release existing effects
	if (white_balance)
	{
		context.Detach(white_balance);
		white_balance.Reset();
	}

	if (simple_tonemap)
	{
		context.Detach(simple_tonemap);
		simple_tonemap.Reset();
	}

	if (tonemap)
	{
		context.Detach(tonemap);
		tonemap.Reset();
	}

	if (normalization)
	{
		context.Detach(normalization);
		normalization.Reset();
	}

	if (gamma_correction)
	{
		context.Detach(gamma_correction);
		gamma_correction.Reset();
	}

	// At least one post effect is required for frame buffer resolve to
	// work, which is required for OpenGL interop. Frame buffer normalization
	// should be applied before other post effects.
	// Also gamma effect requires normalization when tonemapping is not used.
	normalization = frw::PostEffect(context, frw::PostEffectTypeNormalization);
	context.Attach(normalization);

	// Create new effects
	switch (fireRenderGlobalsData.toneMappingType)
	{
	case 0:
		break;

	case 1:
		if (!tonemap)
		{
			tonemap = frw::PostEffect(context, frw::PostEffectTypeToneMap);
			context.Attach(tonemap);
		}
		context.SetParameter(RPR_CONTEXT_TONE_MAPPING_TYPE, RPR_TONEMAPPING_OPERATOR_LINEAR);
		context.SetParameter(RPR_CONTEXT_TONE_MAPPING_LINEAR_SCALE, fireRenderGlobalsData.toneMappingLinearScale);
		break;

	case 2:
		if (!tonemap)
		{
			tonemap = frw::PostEffect(context, frw::PostEffectTypeToneMap);
			context.Attach(tonemap);
		}
		context.SetParameter(RPR_CONTEXT_TONE_MAPPING_TYPE, RPR_TONEMAPPING_OPERATOR_PHOTOLINEAR);
		context.SetParameter(RPR_CONTEXT_TONE_MAPPING_PHOTO_LINEAR_SENSITIVITY, fireRenderGlobalsData.toneMappingPhotolinearSensitivity);
		context.SetParameter(RPR_CONTEXT_TONE_MAPPING_PHOTO_LINEAR_FSTOP, fireRenderGlobalsData.toneMappingPhotolinearFstop);
		context.SetParameter(RPR_CONTEXT_TONE_MAPPING_PHOTO_LINEAR_EXPOSURE, fireRenderGlobalsData.toneMappingPhotolinearExposure);
		break;

	case 3:
		if (!tonemap)
		{
			tonemap = frw::PostEffect(context, frw::PostEffectTypeToneMap);
			context.Attach(tonemap);
		}
		context.SetParameter(RPR_CONTEXT_TONE_MAPPING_TYPE, RPR_TONEMAPPING_OPERATOR_AUTOLINEAR);
		break;

	case 4:
		if (!tonemap)
		{
			tonemap = frw::PostEffect(context, frw::PostEffectTypeToneMap);
			context.Attach(tonemap);
		}
		context.SetParameter(RPR_CONTEXT_TONE_MAPPING_TYPE, RPR_TONEMAPPING_OPERATOR_MAXWHITE);
		break;

	case 5:
		if (!tonemap)
		{
			tonemap = frw::PostEffect(context, frw::PostEffectTypeToneMap);
			context.Attach(tonemap);
		}
		context.SetParameter(RPR_CONTEXT_TONE_MAPPING_TYPE, RPR_TONEMAPPING_OPERATOR_REINHARD02);
		context.SetParameter(RPR_CONTEXT_TONE_MAPPING_REINHARD02_PRE_SCALE, fireRenderGlobalsData.toneMappingReinhard02Prescale);
		context.SetParameter(RPR_CONTEXT_TONE_MAPPING_REINHARD02_POST_SCALE, fireRenderGlobalsData.toneMappingReinhard02Postscale);
		context.SetParameter(RPR_CONTEXT_TONE_MAPPING_REINHARD02_BURN, fireRenderGlobalsData.toneMappingReinhard02Burn);
		break;

	case 6:
		if (!simple_tonemap)
		{
			simple_tonemap = frw::PostEffect(context, frw::PostEffectTypeSimpleTonemap);
			simple_tonemap.SetParameter("tonemap", fireRenderGlobalsData.toneMappingSimpleTonemap);
			simple_tonemap.SetParameter("exposure", fireRenderGlobalsData.toneMappingSimpleExposure);
			simple_tonemap.SetParameter("contrast", fireRenderGlobalsData.toneMappingSimpleContrast);
			context.Attach(simple_tonemap);
		}
		break;

	default:
		break;
	}

	// This block is required for gamma functionality
	if (fireRenderGlobalsData.applyGammaToMayaViews)
	{
		gamma_correction = frw::PostEffect(context, frw::PostEffectTypeGammaCorrection);
		context.Attach(gamma_correction);
	}

	if (fireRenderGlobalsData.toneMappingWhiteBalanceEnabled && !disableWhiteBalance)
	{
		if (!white_balance)
		{
			white_balance = frw::PostEffect(context, frw::PostEffectTypeWhiteBalance);
			context.Attach(white_balance);
		}
		float temperature = fireRenderGlobalsData.toneMappingWhiteBalanceValue;
		white_balance.SetParameter("colorspace", RPR_COLOR_SPACE_SRGB); // check: Max uses Adobe SRGB here
		white_balance.SetParameter("colortemp", temperature);
	}
}

bool TahoeContext::needResolve() const
{
	return bool(white_balance) || bool(simple_tonemap) || bool(tonemap) || bool(normalization) || bool(gamma_correction);
}

bool TahoeContext::IsRenderQualitySupported(RenderQuality quality) const
{
	return quality == RenderQuality::RenderQualityFull;
}
