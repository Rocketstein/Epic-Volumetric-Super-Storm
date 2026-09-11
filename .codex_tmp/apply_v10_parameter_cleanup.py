from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def edit(relative_path: str, transform) -> None:
    path = ROOT / relative_path
    with path.open("r", encoding="utf-8", newline="") as stream:
        original = stream.read()
    newline = "\r\n" if original.count("\r\n") > original.count("\n") / 2 else "\n"

    def normalized(value: str) -> str:
        return value.replace("\n", newline)

    updated = transform(original, normalized)
    if updated == original:
        raise RuntimeError(f"No change produced for {relative_path}")
    with path.open("w", encoding="utf-8", newline="") as stream:
        stream.write(updated)


def replace_once(data: str, old: str, new: str, label: str) -> str:
    count = data.count(old)
    if count != 1:
        raise RuntimeError(f"Expected one {label} block, found {count}")
    return data.replace(old, new, 1)


def replace_between(data: str, start: str, end: str, replacement: str, label: str) -> str:
    if data.count(start) != 1 or data.count(end) != 1:
        raise RuntimeError(f"Could not uniquely locate {label} boundaries")
    start_index = data.index(start)
    end_index = data.index(end, start_index)
    return data[:start_index] + replacement + data[end_index:]


def edit_storm_types(data: str, n) -> str:
    data = replace_once(
        data,
        n('''\
\tUPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm|Shape|Anvil", meta = (ClampMin = "0.01", ClampMax = "8.0"))
\tfloat AnvilWindStretch = 1.8f;

\tUPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm|Shape|Anvil", meta = (ClampMin = "-2.0", ClampMax = "2.0"))
\tfloat AnvilWindSkew = 0.20f;

\tUPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm|Shape|Anvil", meta = (ClampMin = "0.0", ClampMax = "0.999"))
\tfloat AnvilWarpStart01 = 0.62f;

\tUPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm|Shape|Anvil", meta = (ClampMin = "0.001", ClampMax = "1.0"))
\tfloat AnvilWarpEnd01 = 0.86f;

'''),
        "",
        "Anvil wind/warp settings",
    )
    data = replace_once(
        data,
        n('''\
\t// Smooth macro occupancy: a flat interior followed by a gentle body-rim falloff.
\t// Underside tendrils are stored separately and never modify this field.
'''),
        n('''\
\t// Upper bound of the ShapeRT Coverage composite. TwistedModelingNoise only
\t// selects within the existing rim-to-ceiling range.
'''),
        "Coverage ownership comment",
    )
    data = replace_once(
        data,
        "\t// The analytic spiral skew remains fully active through this normalized radius." + n("\n"),
        "\t// The static twist remains fully active through this normalized radius." + n("\n"),
        "full-strength comment",
    )
    data = replace_once(
        data,
        "\t// The analytic spiral skew reaches zero at this normalized radius." + n("\n"),
        "\t// The static twist reaches zero at this normalized radius." + n("\n"),
        "zero-strength comment",
    )
    data = replace_between(
        data,
        n('''\
\t// Single-octave value-noise frequency. This defines the island topology and
'''),
        n('''\
\tUPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm|Shape|Spiral", meta = (ClampMin = "0.0", ClampMax = "65535.0"))
'''),
        n('''\
\t// Legacy serialized names retained for the active TwistedModelingNoise controls.
\t// The base scale sets the smooth macro-noise frequency.
\tUPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm|Shape|Spiral", meta = (ClampMin = "0.001", ClampMax = "128.0", UIMin = "1.0", UIMax = "64.0"))
\tfloat SpiralCarveIslandScale = 16.0f;

\t// Detail frequency relative to SpiralCarveIslandScale.
\tUPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm|Shape|Spiral", meta = (ClampMin = "0.001", ClampMax = "64.0", UIMin = "0.1", UIMax = "8.0"))
\tfloat SpiralCarveDetailScaleMul = 3.3f;

\t// Blend from macro noise to detail noise; it does not add a threshold or carve.
\tUPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm|Shape|Spiral", meta = (ClampMin = "0.0", ClampMax = "1.0"))
\tfloat SpiralCarveDetailAmount = 0.35f;

'''),
        "SpiralTendril settings",
    )
    data = replace_once(
        data,
        n('''\
\tCombine(Settings.AnvilWindStretch);
\tCombine(Settings.AnvilWindSkew);
\tCombine(Settings.AnvilWarpStart01);
\tCombine(Settings.AnvilWarpEnd01);
'''),
        "",
        "Anvil hash entries",
    )
    data = replace_between(
        data,
        n("\tCombine(Settings.SpiralTendrilIslandScale);\n"),
        n("\tCombine(Settings.SpiralSeed);\n"),
        n('''\
\tCombine(Settings.SpiralCarveIslandScale);
\tCombine(Settings.SpiralCarveDetailScaleMul);
\tCombine(Settings.SpiralCarveDetailAmount);
'''),
        "SpiralTendril hash entries",
    )
    return data


def edit_actor(data: str, n) -> str:
    data = replace_between(
        data,
        n("\tSanitizedShape.AnvilWindStretch ="),
        n("\tSanitizedShape.ShapeTwistRadians ="),
        "",
        "Anvil sanitizer",
    )
    data = replace_between(
        data,
        n("\tSanitizedShape.SpiralTendrilIslandScale ="),
        n("\tSanitizedShape.SpiralSeed ="),
        n('''\
\tSanitizedShape.SpiralCarveIslandScale = FMath::Clamp(Shape.SpiralCarveIslandScale, 0.001f, 128.0f);
\tSanitizedShape.SpiralCarveDetailScaleMul = FMath::Clamp(Shape.SpiralCarveDetailScaleMul, 0.001f, 64.0f);
\tSanitizedShape.SpiralCarveDetailAmount = FMath::Clamp(Shape.SpiralCarveDetailAmount, 0.0f, 1.0f);
'''),
        "SpiralTendril sanitizer",
    )
    return data


def edit_shader_header(data: str, n) -> str:
    return replace_between(
        data,
        n("\tfloat SpiralTendrilIslandScale ="),
        n("\tfloat SpiralSeed ="),
        n('''\
\tfloat SpiralCarveIslandScale = 16.0f;
\tfloat SpiralCarveDetailScaleMul = 3.3f;
\tfloat SpiralCarveDetailAmount = 0.35f;
'''),
        "shape-pass SpiralTendril fields",
    )


def edit_shader_cpp(data: str, n) -> str:
    data = replace_between(
        data,
        n("\t\tSHADER_PARAMETER(float, SpiralTendrilIslandScale)\n"),
        n("\t\tSHADER_PARAMETER(float, SpiralSeed)\n"),
        n('''\
\t\tSHADER_PARAMETER(float, SpiralCarveIslandScale)
\t\tSHADER_PARAMETER(float, SpiralCarveDetailScaleMul)
\t\tSHADER_PARAMETER(float, SpiralCarveDetailAmount)
'''),
        "shader SpiralTendril parameters",
    )
    data = replace_between(
        data,
        n("\tPassParameters->SpiralTendrilIslandScale ="),
        n("\tPassParameters->SpiralSeed ="),
        n('''\
\tPassParameters->SpiralCarveIslandScale = InPassParameters.SpiralCarveIslandScale;
\tPassParameters->SpiralCarveDetailScaleMul = InPassParameters.SpiralCarveDetailScaleMul;
\tPassParameters->SpiralCarveDetailAmount = InPassParameters.SpiralCarveDetailAmount;
'''),
        "shader SpiralTendril assignments",
    )
    return data


def edit_shape_binding(data: str, n) -> str:
    return replace_between(
        data,
        n("\tPassParameters.SpiralTendrilIslandScale ="),
        n("\tPassParameters.SpiralSeed ="),
        n('''\
\tPassParameters.SpiralCarveIslandScale = Shape.SpiralCarveIslandScale;
\tPassParameters.SpiralCarveDetailScaleMul = Shape.SpiralCarveDetailScaleMul;
\tPassParameters.SpiralCarveDetailAmount = Shape.SpiralCarveDetailAmount;
'''),
        "shape-pass SpiralTendril binding",
    )


def edit_material_parameter_header(data: str, n) -> str:
    data = replace_once(
        data,
        n('''\
\tinline const FName AnvilWindDirectionXY(TEXT("AnvilWindDirectionXY"));
\tinline const FName AnvilWindStretch(TEXT("AnvilWindStretch"));
\tinline const FName AnvilWindSkew(TEXT("AnvilWindSkew"));
\tinline const FName AnvilWarpStart01(TEXT("AnvilWarpStart01"));
\tinline const FName AnvilWarpEnd01(TEXT("AnvilWarpEnd01"));
\tinline const FName RotationSign(TEXT("RotationSign"));
'''),
        n('''\
\tinline const FName RotationSign(TEXT("RotationSign"));
\tinline const FName OuterBrimRadiusScale(TEXT("OuterBrimRadiusScale"));
\tinline const FName ShapeTwistRadians(TEXT("ShapeTwistRadians"));
\tinline const FName SpiralTwistPower(TEXT("SpiralTwistPower"));
\tinline const FName SpiralFullStrengthRadius01(TEXT("SpiralFullStrengthRadius01"));
\tinline const FName SpiralZeroStrengthRadius01(TEXT("SpiralZeroStrengthRadius01"));
'''),
        "material Anvil wind/twist names",
    )
    data = replace_once(
        data,
        "\t// density contract. Static spiral warp parameters no longer exist in CL5." + n("\n"),
        n('''\
\t// density contract. The material reuses the ShapeRT static-twist controls
\t// for Anvil rotation; no Anvil-specific transform parameters are exposed.
'''),
        "material contract comment",
    )
    return data


def edit_material_parameter_cpp(data: str, n) -> str:
    data = replace_once(
        data,
        n('''\
\tTArray<FMaterialParameterInfo> VectorInfos;
\tTArray<FGuid> VectorIds;
\tMaterial->GetAllVectorParameterInfo(VectorInfos, VectorIds);

'''),
        "",
        "unused vector parameter query",
    )
    data = replace_once(
        data,
        n('''\
\treturn ContainsParameter(TextureInfos, ShapeTexture)
\t\t&& ContainsParameter(TextureInfos, ShapeTexture2)
\t\t&& ContainsParameter(VectorInfos, AnvilWindDirectionXY)
\t\t&& ContainsParameter(ScalarInfos, AnvilStrength)
\t\t&& ContainsParameter(ScalarInfos, AnvilTypeBias)
\t\t&& ContainsParameter(ScalarInfos, AnvilCeilingFade01)
\t\t&& ContainsParameter(ScalarInfos, AnvilWindStretch)
\t\t&& ContainsParameter(ScalarInfos, AnvilWindSkew)
\t\t&& ContainsParameter(ScalarInfos, AnvilWarpStart01)
\t\t&& ContainsParameter(ScalarInfos, AnvilWarpEnd01)
\t\t&& ContainsParameter(ScalarInfos, RotationSign)
'''),
        n('''\
\treturn ContainsParameter(TextureInfos, ShapeTexture)
\t\t&& ContainsParameter(TextureInfos, ShapeTexture2)
\t\t&& ContainsParameter(ScalarInfos, AnvilStrength)
\t\t&& ContainsParameter(ScalarInfos, AnvilTypeBias)
\t\t&& ContainsParameter(ScalarInfos, AnvilCeilingFade01)
\t\t&& ContainsParameter(ScalarInfos, RotationSign)
\t\t&& ContainsParameter(ScalarInfos, OuterBrimRadiusScale)
\t\t&& ContainsParameter(ScalarInfos, ShapeTwistRadians)
\t\t&& ContainsParameter(ScalarInfos, SpiralTwistPower)
\t\t&& ContainsParameter(ScalarInfos, SpiralFullStrengthRadius01)
\t\t&& ContainsParameter(ScalarInfos, SpiralZeroStrengthRadius01)
'''),
        "required Anvil wind/twist parameters",
    )
    return data


def edit_material_binder(data: str, n) -> str:
    data = replace_once(
        data,
        n('''\
\tconst float AnvilWarpStart01 = FMath::Clamp(Shape.AnvilWarpStart01, 0.0f, 0.999f);
\tconst float AnvilWarpEnd01 = FMath::Clamp(Shape.AnvilWarpEnd01, AnvilWarpStart01 + 1.0e-4f, 1.0f);
'''),
        n('''\
\tconst float SpiralFullStrengthRadius01 = FMath::Clamp(Shape.SpiralFullStrengthRadius01, 0.0f, 0.999f);
\tconst float SpiralZeroStrengthRadius01 = FMath::Clamp(
\t\tShape.SpiralZeroStrengthRadius01,
\t\tSpiralFullStrengthRadius01 + 1.0e-4f,
\t\t1.0f);
'''),
        "binder Anvil warp locals",
    )
    data = replace_once(
        data,
        n('''\
\tDynamicCloudMaterial->SetScalarParameterValue(
\t\tCloudMaterialParams::RotationSign,
\t\tShape.bClockwise ? 1.0f : -1.0f);
'''),
        n('''\
\tDynamicCloudMaterial->SetScalarParameterValue(
\t\tCloudMaterialParams::RotationSign,
\t\tShape.bClockwise ? 1.0f : -1.0f);
\tDynamicCloudMaterial->SetScalarParameterValue(
\t\tCloudMaterialParams::OuterBrimRadiusScale,
\t\tFMath::Max(FMath::Abs(Shape.OuterBrimRadiusScale), 1.0e-4f));
\tDynamicCloudMaterial->SetScalarParameterValue(
\t\tCloudMaterialParams::ShapeTwistRadians,
\t\tFMath::Clamp(Shape.ShapeTwistRadians, 0.0f, 12.566f));
\tDynamicCloudMaterial->SetScalarParameterValue(
\t\tCloudMaterialParams::SpiralTwistPower,
\t\tFMath::Clamp(Shape.SpiralTwistPower, 1.0e-3f, 4.0f));
\tDynamicCloudMaterial->SetScalarParameterValue(
\t\tCloudMaterialParams::SpiralFullStrengthRadius01,
\t\tSpiralFullStrengthRadius01);
\tDynamicCloudMaterial->SetScalarParameterValue(
\t\tCloudMaterialParams::SpiralZeroStrengthRadius01,
\t\tSpiralZeroStrengthRadius01);
'''),
        "binder shared twist uploads",
    )
    data = replace_between(
        data,
        n("\tDynamicCloudMaterial->SetVectorParameterValue(\n\t\tCloudMaterialParams::AnvilWindDirectionXY,"),
        n("}\n\nvoid UStormMaterialBinderComponent::UploadPerFrameParamsToMID"),
        "",
        "binder Anvil wind/warp uploads",
    )
    return data


def edit_debug(data: str, n) -> str:
    data = replace_once(
        data,
        "StormShapeRT2 (MinLayerH01, SpiralTendrilMask, MaxLayerH01, CenterMask)",
        "StormShapeRT2 (MinLayerH01, TwistedModelingNoise, MaxLayerH01, CenterMask)",
        "ShapeRT2 console help label",
    )
    data = replace_once(
        data,
        'TEXT("SpiralTendrilMask")',
        'TEXT("TwistedModelingNoise")',
        "ShapeRT2 channel label",
    )
    return data


edit("Plugins/SavageSuperStorm/Source/SavageSuperStormRuntime/Public/Data/StormTypes.h", edit_storm_types)
edit("Plugins/SavageSuperStorm/Source/SavageSuperStormRuntime/Private/Actors/VolumetricSuperStormActor.cpp", edit_actor)
edit("Plugins/SavageSuperStorm/Source/SavageSuperStormShaders/Public/Shader_SavageSuperStorm.h", edit_shader_header)
edit("Plugins/SavageSuperStorm/Source/SavageSuperStormShaders/Private/Shader_SavageSuperStorm.cpp", edit_shader_cpp)
edit("Plugins/SavageSuperStorm/Source/SavageSuperStormRuntime/Private/Subsystems/CloudInteractionWorldSubsystem.cpp", edit_shape_binding)
edit("Plugins/SavageSuperStorm/Source/SavageSuperStormRuntime/Private/Material/StormCloudMaterialParameters.h", edit_material_parameter_header)
edit("Plugins/SavageSuperStorm/Source/SavageSuperStormRuntime/Private/Material/StormCloudMaterialParameters.cpp", edit_material_parameter_cpp)
edit("Plugins/SavageSuperStorm/Source/SavageSuperStormRuntime/Private/Components/StormMaterialBinderComponent.cpp", edit_material_binder)
edit("Plugins/SavageSuperStorm/Source/SavageSuperStormRuntime/Private/Subsystems/DebugSubsystem.cpp", edit_debug)

print("CODEX_V10_PARAMETER_CLEANUP|SUCCESS")
