#include "pch.h"
#include "particle_operators_params.h"
#include <public/dmexchange.h>
#include <public/particle_effect.h>


template <typename T>
T ParticleEffect_FindAndSetField_OrDefault(DmSymbolTable& symbolTable, const DmElement_s& elem, const std::string_view name, const T defaultValue)
{
    const DmSymbolTable::SymbolId_t sym = symbolTable.Find(name);

    if (sym == DmSymbolTable::npos)
        return defaultValue;

    const DmAttribute_s* const at = Dme_FindAttribute(elem, DmeSymbol_t(sym));

    if (!at)
        return defaultValue;

    const DmAttributeType_e expectedType = Dma_GetTypeForPod<T>();

    if (at->type != expectedType)
        return defaultValue;

    return Dma_GetValue<T>(*at);
}

template <typename T>
T ParticleEffect_FindAndSetField_OrDefault_Clamped(DmSymbolTable& symbolTable, const DmElement_s& elem, const std::string_view name, const T defaultValue, const T minValue, const T maxValue)
{
    const T result = ParticleEffect_FindAndSetField_OrDefault<T>(symbolTable, elem, name, defaultValue);

    if (result < minValue || result > maxValue)
    {
        // todo: warning.
        return defaultValue;
    }

    return result;
}

/*static*/ void ParticleEffect_BakeParams(DmSymbolTable& symbolTable, const DmElement_s& elem, ParticleDefintionParams_s& params)
{
    params.maxParticles = ParticleEffect_FindAndSetField_OrDefault(symbolTable, elem, "max_particles", 1000);
    params.initialParticles = ParticleEffect_FindAndSetField_OrDefault(symbolTable, elem, "initial_particles", 0);

    // todo: materialName strings

    params.boundingBoxMin = ParticleEffect_FindAndSetField_OrDefault(symbolTable, elem, "bounding_box_min", Vector3(-10, -10, -10));
    params.boundingBoxMax = ParticleEffect_FindAndSetField_OrDefault(symbolTable, elem, "bounding_box_max", Vector3(10, 10, 10));

    params.cullRadius = ParticleEffect_FindAndSetField_OrDefault(symbolTable, elem, "cull_radius", 0.0f);
    params.cullControlPoint = ParticleEffect_FindAndSetField_OrDefault_Clamped(symbolTable, elem, "cull_control_point", 0, 0, 63);

    // todo: fallbackReplacementName strings

    params.fallbackThreshold = ParticleEffect_FindAndSetField_OrDefault(symbolTable, elem, "fallback threshold", 1); // todo: is this a float? most likely not, check s0 pak

    params.constantRadius = ParticleEffect_FindAndSetField_OrDefault(symbolTable, elem, "radius", 5.0f);
    params.constantColor = ParticleEffect_FindAndSetField_OrDefault(symbolTable, elem, "color", ColorB(255, 255, 255, 255));
    params.constantColorScale = ParticleEffect_FindAndSetField_OrDefault(symbolTable, elem, "color HDR scale", 1.0f);
    params.constantRotation = ParticleEffect_FindAndSetField_OrDefault(symbolTable, elem, "rotation", 0.0f);
    params.constantRotationSpeed = ParticleEffect_FindAndSetField_OrDefault(symbolTable, elem, "rotation_speed", 0.0f);
    params.constantNormal = ParticleEffect_FindAndSetField_OrDefault(symbolTable, elem, "normal", Vector3(0, 0, 1));
    params.constantNormalSpinDegrees = ParticleEffect_FindAndSetField_OrDefault(symbolTable, elem, "normal spin degrees", 0.0f);
    // todo: figure out sequence numbers (user data???)

    params.groupId = ParticleEffect_FindAndSetField_OrDefault(symbolTable, elem, "group id", 0);
    params.maximumTimeStep = ParticleEffect_FindAndSetField_OrDefault(symbolTable, elem, "maximum time step", 0.1f);
    params.maximumSimTime = ParticleEffect_FindAndSetField_OrDefault(symbolTable, elem, "maximum sim tick rate", 0.0f);
    params.minimumSimTime = ParticleEffect_FindAndSetField_OrDefault(symbolTable, elem, "minimum sim tick rate", 0.0f);
    params.minimumFrames = ParticleEffect_FindAndSetField_OrDefault(symbolTable, elem, "minimum rendered frames", 0);

    params.skipRenderControlPoint = ParticleEffect_FindAndSetField_OrDefault_Clamped(symbolTable, elem, "control point to disable rendering if it is the camera", -1, -1, 63);
    params.allowRenderControlPoint = ParticleEffect_FindAndSetField_OrDefault_Clamped(symbolTable, elem, "control point to only enable rendering if it is the camera", -1, -1, 63);

    params.maxDrawDistance = ParticleEffect_FindAndSetField_OrDefault(symbolTable, elem, "maximum draw distance", 100000.0f);
    params.minDrawDistance = ParticleEffect_FindAndSetField_OrDefault(symbolTable, elem, "minimum draw distance", 0.0f);

    params.applyDrawDistanceWhenChild = ParticleEffect_FindAndSetField_OrDefault(symbolTable, elem, "apply draw distance when child", false);
    params.noDrawTimeToGoToSleep = ParticleEffect_FindAndSetField_OrDefault(symbolTable, elem, "time to sleep when not drawn", 8.0f);

    params.shouldSort = ParticleEffect_FindAndSetField_OrDefault(symbolTable, elem, "Sort particles", true);
    params.shouldBatch = ParticleEffect_FindAndSetField_OrDefault(symbolTable, elem, "batch particle systems", false);

    params.viewModelEffect = ParticleEffect_FindAndSetField_OrDefault(symbolTable, elem, "view model effect", false);
    params.screenSpaceEffect = ParticleEffect_FindAndSetField_OrDefault(symbolTable, elem, "screen space effect", false);
    params.drawWithScreenSpace = ParticleEffect_FindAndSetField_OrDefault(symbolTable, elem, "draw with screen space", false);
    params.drawThroughLeafSystem = ParticleEffect_FindAndSetField_OrDefault(symbolTable, elem, "draw through leafsystem", true);
    params.checkOwnerDormant = ParticleEffect_FindAndSetField_OrDefault(symbolTable, elem, "check owner dormant", true);
    params.maxRecursionDepth = ParticleEffect_FindAndSetField_OrDefault(symbolTable, elem, "maximum portal recursion depth", 8);
    params.aggregateRadius = ParticleEffect_FindAndSetField_OrDefault(symbolTable, elem, "aggregation radius", 0.0f);
    params.aggregationMinAvailableParticles = ParticleEffect_FindAndSetField_OrDefault(symbolTable, elem, "minimum free particles to aggregate", 0);
    params.minimumTimeStep = ParticleEffect_FindAndSetField_OrDefault(symbolTable, elem, "minimum simulation time step", 0.0f);
    params.minCpuLevel = ParticleEffect_FindAndSetField_OrDefault(symbolTable, elem, "minimum CPU level", 0);
    params.minGpuLevel = ParticleEffect_FindAndSetField_OrDefault(symbolTable, elem, "minimum GPU level", 0);

    params.stopSimulationAfterTime = ParticleEffect_FindAndSetField_OrDefault(symbolTable, elem, "freeze simulation after time", 1000000000.0f);
    params.warmUpTime = ParticleEffect_FindAndSetField_OrDefault(symbolTable, elem, "warm-up time", 0.0f);
    params.pauseAfterWarmUp = ParticleEffect_FindAndSetField_OrDefault(symbolTable, elem, "warm-up and pause", false);

    params.killIfOverLimit = ParticleEffect_FindAndSetField_OrDefault(symbolTable, elem, "kill if over-limit", true);
    params.useHeightInYaw = ParticleEffect_FindAndSetField_OrDefault(symbolTable, elem, "use height in yaw", false);
    params.doDrawDuringPhaseShift = ParticleEffect_FindAndSetField_OrDefault(symbolTable, elem, "phase shift - allowed", false);
    params.doOnlyDrawDuringPhaseShift = ParticleEffect_FindAndSetField_OrDefault(symbolTable, elem, "phase shift - only", false);
    params.inheritsAlphaVisibility = ParticleEffect_FindAndSetField_OrDefault(symbolTable, elem, "inherit alpha visibility", false);
    params.inheritsRawVisibility = ParticleEffect_FindAndSetField_OrDefault(symbolTable, elem, "inherit proxy visibility", false);
    params.randomSeed = ParticleEffect_FindAndSetField_OrDefault(symbolTable, elem, "random seed override", 0);

    params.renderShadows = ParticleEffect_FindAndSetField_OrDefault(symbolTable, elem, "shadows", false);
    params.isScripted = ParticleEffect_FindAndSetField_OrDefault(symbolTable, elem, "scripted", false);
    params.reserved = ParticleEffect_FindAndSetField_OrDefault(symbolTable, elem, "reserved", false);
    params.sortFromOrigin = ParticleEffect_FindAndSetField_OrDefault(symbolTable, elem, "sort from origin", false);

    params.lowResDrawEnable = ParticleEffect_FindAndSetField_OrDefault(symbolTable, elem, "low res draw enable", false);
    params.lowResDist = ParticleEffect_FindAndSetField_OrDefault(symbolTable, elem, "low res draw dist", 800.0f);

    params.inheritEntityScale = ParticleEffect_FindAndSetField_OrDefault(symbolTable, elem, "Inherit entity scale", true);
}

/*static*/ void ParticleEffect_BakeParams_C_OP_GraphVector_Params(DmSymbolTable& symbolTable, const DmElement_s& elem, C_OP_GraphVector_Params& params)
{
    // TODO: graph array
    /*
        graph: "0 0 0 0 0.5 1 1 1 1 0 0 0"
    */
    params.m_isLooping = ParticleEffect_FindAndSetField_OrDefault(symbolTable, elem, "graph loop", false);
    params.m_graphTime = ParticleEffect_FindAndSetField_OrDefault(symbolTable, elem, "graph time", 1.0f);
    params.m_graphTimeIsInLifespans = ParticleEffect_FindAndSetField_OrDefault(symbolTable, elem, "graph time is in lifespans", true);
    params.m_bUseLocalSpace = ParticleEffect_FindAndSetField_OrDefault(symbolTable, elem, "use local space", false);
    params.m_nFieldOutput = ParticleEffect_FindAndSetField_OrDefault(symbolTable, elem, "output field", 0);
    params.m_outputScaleType = ParticleEffect_FindAndSetField_OrDefault(symbolTable, elem, "output op", 0);
    params.m_LocalCtrlPoint = ParticleEffect_FindAndSetField_OrDefault_Clamped(symbolTable, elem, "local control point", 0, 0, 63);
    params.m_inputTimeCtrlPoint = ParticleEffect_FindAndSetField_OrDefault_Clamped(symbolTable, elem, "input time control point", -1, -1, 63);
    params.m_vecOutputMin = ParticleEffect_FindAndSetField_OrDefault(symbolTable, elem, "output minimum", Vector3(-1.0f, -1.0f, -1.0f));
    params.m_vecOutputMax = ParticleEffect_FindAndSetField_OrDefault(symbolTable, elem, "output maximum", Vector3(1.0f, 1.0f, 1.0f));
}

/*static*/ void ParticleEffect_BakeParams_C_OP_RenderLightSource_Params(DmSymbolTable& symbolTable, const DmElement_s& elem, C_OP_RenderLightSource_Params& params)
{
    params.m_flColorScale = ParticleEffect_FindAndSetField_OrDefault(symbolTable, elem, "color scale", 1.0f);
    params.m_bColorScaleByAlpha = ParticleEffect_FindAndSetField_OrDefault(symbolTable, elem, "color scale by alpha", false);
    params.m_flRadiusScale = ParticleEffect_FindAndSetField_OrDefault(symbolTable, elem, "radius scale", 1.0f);
    params.m_bIsCockpit = ParticleEffect_FindAndSetField_OrDefault(symbolTable, elem, "is cockpit light", false);
    params.m_flPriorityMultiplier = ParticleEffect_FindAndSetField_OrDefault(symbolTable, elem, "priority multiplier", 1.0f);
    params.m_flFalloffHalfwayFrac = ParticleEffect_FindAndSetField_OrDefault(symbolTable, elem, "falloff halfway fraction", 0.3f);
}

/*static*/ void ParticleEffect_BakeParams_C_OP_RenderScreenVelocityRotate_Params(DmSymbolTable& symbolTable, const DmElement_s& elem, C_OP_RenderScreenVelocityRotate_Params& params)
{
    params.m_flRotateRateDegrees = ParticleEffect_FindAndSetField_OrDefault(symbolTable, elem, "rotate_rate(dps)", 0.0f);
    params.m_flForwardDegrees = ParticleEffect_FindAndSetField_OrDefault(symbolTable, elem, "forward_angle", -90.0f);
}
