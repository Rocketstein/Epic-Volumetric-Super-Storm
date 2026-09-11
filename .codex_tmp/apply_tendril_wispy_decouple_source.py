from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def edit(relative_path, replacements=(), slicers=()):
    path = ROOT / relative_path
    raw = path.read_bytes()
    crlf = b"\r\n" in raw
    text = raw.decode("utf-8").replace("\r\n", "\n")
    for old, new in replacements:
        count = text.count(old)
        if count != 1:
            raise RuntimeError(
                f"{relative_path}: expected one occurrence, found {count}: {old[:80]!r}"
            )
        text = text.replace(old, new, 1)
    for start_marker, end_marker, replacement in slicers:
        start = text.find(start_marker)
        if start < 0:
            raise RuntimeError(f"{relative_path}: start marker missing: {start_marker!r}")
        end = text.find(end_marker, start)
        if end < 0:
            raise RuntimeError(f"{relative_path}: end marker missing: {end_marker!r}")
        text = text[:start] + replacement + text[end:]
    if crlf:
        text = text.replace("\n", "\r\n")
    path.write_bytes(text.encode("utf-8"))


edit(
    "Plugins/SavageSuperStorm/Shaders/Public/StormDensityMaterialAdapter.ush",
    replacements=(
        (
            "    float StormStencil,\n"
            "    float SpiralTendrilMask,\n"
            "    float SpiralTendrilStrength,\n"
            "    float3 WorldPosition,",
            "    float StormStencil,\n"
            "    float3 WorldPosition,",
        ),
        (
            "    // The tendril is morphology demand, not occupancy. BaseBottomType also\n"
            "    // supplies its strength, while the lower-band extent stays fixed.\n"
            "    float undersideBand = StormEvaluateSpiralUndersideBand(\n"
            "        h,\n"
            "        BaseBottomType);\n"
            "    float effectiveBottomType = lerp(\n"
            "        saturate(BaseBottomType),\n"
            "        1.0,\n"
            "        saturate(SpiralTendrilMask)\n"
            "            * saturate(SpiralTendrilStrength)\n"
            "            * undersideBand\n"
            "            * saturate(domain.SuperstormCoverage));\n\n"
            "    // EffectiveBottomType belongs only to the normal underside path.\n",
            "    // Wispy selection is owned exclusively by the authored BaseBottomType.\n"
            "    // ShapeRT2.G (SpiralTendrilMask) is intentionally not part of density.\n",
        ),
        (
            "        h,\n"
            "        effectiveBottomType);",
            "        h,\n"
            "        BaseBottomType);",
        ),
    ),
)

# The same local was passed to all three lifecycle noise samples.
adapter = ROOT / "Plugins/SavageSuperStorm/Shaders/Public/StormDensityMaterialAdapter.ush"
raw = adapter.read_bytes()
crlf = b"\r\n" in raw
text = raw.decode("utf-8").replace("\r\n", "\n")
count = text.count("            effectiveBottomType,")
if count != 2:
    raise RuntimeError(f"Expected two indented effectiveBottomType uses, found {count}")
text = text.replace("            effectiveBottomType,", "            BaseBottomType,")
count = text.count("        effectiveBottomType,")
if count != 1:
    raise RuntimeError(f"Expected one final effectiveBottomType use, found {count}")
text = text.replace("        effectiveBottomType,", "        BaseBottomType,")
if crlf:
    text = text.replace("\n", "\r\n")
adapter.write_bytes(text.encode("utf-8"))

edit(
    "Plugins/SavageSuperStorm/Shaders/Public/StormDensityField.ush",
    replacements=(
        (
            "// EffectiveBottomType selects weakly correlated underside HF carriers inside a\n"
            "// fixed lower band. It never expands the band or adds a second remap.",
            "// BaseBottomType selects weakly correlated underside HF carriers inside a\n"
            "// fixed lower band. SpiralTendrilMask never affects the Wispy/HF path.",
        ),
        (
            "float StormEvaluateSpiralUndersideBand(\n"
            "    float HLocal,\n"
            "    float BaseBottomType)\n"
            "{\n"
            "    // BottomType controls demand/strength only. The lower-band extent remains\n"
            "    // fixed at the former maximum (0.30) for every column.\n"
            "    return (1.0 - StormEvaluateWispyToBillowyBlend(HLocal))\n"
            "        * StormEvaluateBottomTypeWispyAmount(BaseBottomType);\n"
            "}\n\n",
            "",
        ),
    ),
)

edit(
    "Plugins/SavageSuperStorm/Source/SavageSuperStormRuntime/Public/Field/StormDensityContract.h",
    replacements=(
        ("inline constexpr float DefaultSpiralTendrilStrength = 0.90f;\n", ""),
        (
            "//   UndersideBand = (1 - fixed-height blend) * saturate(BaseBottomType)\n"
            "//   EffectiveBottomType = lerp(BaseBottomType, 1,\n"
            "//       SpiralTendrilMask * SpiralTendrilStrength * UndersideBand\n"
            "//       * SuperstormCoverage)\n"
            "//   BodyProfile = BottomProfile(EffectiveBottomType, HLocal)\n",
            "//   WispyAmount = saturate(BaseBottomType) inside the fixed lower band\n"
            "//   SpiralTendrilMask is stored in ShapeRT2.G but does not feed density\n"
            "//   BodyProfile = BottomProfile(BaseBottomType, HLocal)\n",
        ),
        (
            "inline float EvaluateSpiralUndersideBand(\n"
            "    float HLocal,\n"
            "    float BaseBottomType)\n"
            "{\n"
            "    return (1.0f - EvaluateWispyToBillowyBlend(HLocal))\n"
            "        * Saturate(BaseBottomType);\n"
            "}\n\n",
            "",
        ),
        (
            "    return EvaluateSpiralUndersideBand(HLocal, BaseBottomType);",
            "    return (1.0f - EvaluateWispyToBillowyBlend(HLocal))\n"
            "        * Saturate(BaseBottomType);",
        ),
        (
            "inline float EvaluateEffectiveBottomType(\n"
            "    float BaseBottomType,\n"
            "    float SpiralTendrilMask,\n"
            "    float SpiralTendrilStrength,\n"
            "    float UndersideBand,\n"
            "    float SuperstormCoverage)\n"
            "{\n"
            "    const float Influence = Saturate(\n"
            "        Saturate(SpiralTendrilMask)\n"
            "        * Saturate(SpiralTendrilStrength)\n"
            "        * Saturate(UndersideBand)\n"
            "        * Saturate(SuperstormCoverage));\n"
            "    return FMath::Lerp(Saturate(BaseBottomType), 1.0f, Influence);\n"
            "}\n\n",
            "",
        ),
    ),
)

edit(
    "Plugins/SavageSuperStorm/Source/SavageSuperStormRuntime/Public/Data/StormTypes.h",
    replacements=(
        (
            "\t// Material-only gain that converts the static mask into underside morphology.\n"
            "\tUPROPERTY(EditAnywhere, BlueprintReadWrite, Category = \"Storm|Shape|Spiral\", meta = (ClampMin = \"0.0\", ClampMax = \"1.0\"))\n"
            "\tfloat SpiralTendrilStrength = 0.90f;\n\n",
            "",
        ),
        ("\tCombine(Settings.SpiralTendrilStrength);\n", ""),
    ),
)

edit(
    "Plugins/SavageSuperStorm/Source/SavageSuperStormRuntime/Private/Actors/VolumetricSuperStormActor.cpp",
    replacements=(
        (
            "\tSanitizedShape.SpiralTendrilStrength = FMath::Clamp(\n"
            "\t\tShape.SpiralTendrilStrength, 0.0f, 1.0f);\n",
            "",
        ),
    ),
)

edit(
    "Plugins/SavageSuperStorm/Source/SavageSuperStormRuntime/Private/Components/StormMaterialBinderComponent.cpp",
    replacements=(
        (
            "\tDynamicCloudMaterial->SetScalarParameterValue(\n"
            "\t\tCloudMaterialParams::SpiralTendrilStrength,\n"
            "\t\tFMath::Clamp(Shape.SpiralTendrilStrength, 0.0f, 1.0f));\n",
            "",
        ),
    ),
)

edit(
    "Plugins/SavageSuperStorm/Source/SavageSuperStormRuntime/Private/Material/StormCloudMaterialParameters.h",
    replacements=(("\tinline const FName SpiralTendrilStrength(TEXT(\"SpiralTendrilStrength\"));\n", ""),),
)

edit(
    "Plugins/SavageSuperStorm/Source/SavageSuperStormRuntime/Private/Material/StormCloudMaterialParameters.cpp",
    replacements=(("\t\t&& ContainsParameter(ScalarInfos, SpiralTendrilStrength)\n", ""),),
)

test_file = "Plugins/SavageSuperStorm/Source/SavageSuperStormRuntime/Private/Tests/StormDensityContractTests.cpp"
edit(
    test_file,
    replacements=(
        ("EvaluateSpiralUndersideBand(0.0f, 0.0f)", "EvaluateUndersideCurlWeight(0.0f, 0.0f)"),
        ("EvaluateSpiralUndersideBand(0.0f, 0.5f)", "EvaluateUndersideCurlWeight(0.0f, 0.5f)"),
        ("EvaluateSpiralUndersideBand(0.0f, 1.0f)", "EvaluateUndersideCurlWeight(0.0f, 1.0f)"),
        ("EvaluateSpiralUndersideBand(0.36f, 1.0f)", "EvaluateUndersideCurlWeight(0.36f, 1.0f)"),
        (
            "    TestEqual(TEXT(\"Tendril strength defaults to 0.90\"),\n"
            "        DefaultSpiralTendrilStrength, 0.90f);\n",
            "",
        ),
    ),
    slicers=(
        (
            "float EvaluateMockDensity(\n",
            "}\n\nIMPLEMENT_SIMPLE_AUTOMATION_TEST(\n    FStormDensityHeightContractTest,",
            "",
        ),
        (
            "    const float BaseBottomType = 0.35f;\n",
            "    TestEqual(TEXT(\"AnvilStrength zero is a no-op\"),",
            "    // ShapeRT2.G remains a baked/debug signal only; the active density\n"
            "    // and Wispy contracts expose no Tendril mask or strength input.\n",
        ),
    ),
)

edit(
    "Docs/NubisMesocycloneV9MaterialWiring.md",
    replacements=(
        (
            "`M_SSS.uasset`와 `MF_Erosion`은 바이너리 에셋이므로, 기존 사용자 변경을 보존하기 위해 코드에서 자동 수정하지 않는다. 아래 순서대로 Material Editor에서 Custom 노드 핀과 호출부를 갱신해야 한다.",
            "`M_SSS.uasset`와 `MF_Erosion`은 아래 계약으로 갱신되어 있다.",
        ),
        (
            "`ShapeRT2.G`는 높이 또는 density mask가 아니다. `SpiralTendrilMask`라는 underside morphology 입력으로만 사용한다.",
            "`ShapeRT2.G`의 `SpiralTendrilMask`는 bake/debug 데이터로만 유지한다. 현재 density, Wispy, Curl 경로에는 연결하지 않는다.",
        ),
        ("12. `SpiralTendrilMask` (`float`)\n13. `SpiralTendrilStrength` (`float`, default `0.90`)\n", ""),
        ("14. `WorldPosition`", "12. `WorldPosition`"),
        ("15. `StormCenterRadius`", "13. `StormCenterRadius`"),
        ("16. `LFUVW`", "14. `LFUVW`"),
        ("17. `HFUVW`", "15. `HFUVW`"),
        ("18. `CurlUVW`", "16. `CurlUVW`"),
        ("19. `LFUnitsPerBodyRadius`", "17. `LFUnitsPerBodyRadius`"),
        ("20. `HFUnitsPerBodyRadius`", "18. `HFUnitsPerBodyRadius`"),
        ("21. `CurlUnitsPerBodyRadius`", "19. `CurlUnitsPerBodyRadius`"),
        ("22. `CurlTiling`", "20. `CurlTiling`"),
        ("23. `CurlDisplacementUVW`", "21. `CurlDisplacementUVW`"),
        ("24. `StormLifecycleControl`", "22. `StormLifecycleControl`"),
        ("25. `RotationSign`", "23. `RotationSign`"),
        ("26. `AnvilStrength`", "24. `AnvilStrength`"),
        ("27. `AnvilTypeBias`", "25. `AnvilTypeBias`"),
        ("28. `AnvilCeilingFade01`", "26. `AnvilCeilingFade01`"),
        ("29. `AnvilWindDirectionXY`", "27. `AnvilWindDirectionXY`"),
        ("30. `AnvilWindStretch`", "28. `AnvilWindStretch`"),
        ("31. `AnvilWindSkew`", "29. `AnvilWindSkew`"),
        ("32. `AnvilWarpStart01`", "30. `AnvilWarpStart01`"),
        ("33. `AnvilWarpEnd01`", "31. `AnvilWarpEnd01`"),
        ("34. `DensityGamma`", "32. `DensityGamma`"),
        ("35. `HFStrength`", "33. `HFStrength`"),
        (
            "    StormStencil, SpiralTendrilMask, SpiralTendrilStrength,\n"
            "    WorldPosition,",
            "    StormStencil, WorldPosition,",
        ),
        (
            "2. ShapeRT2의 R/B는 기존 height decode에 유지하고, G를 `SpiralTendrilMask`에 연결한다. A는 `CenterMask` 디버그/기존 소유 경로만 유지한다.\n"
            "3. `SpiralTendrilStrength` scalar parameter를 만들고 기본값을 `0.90`으로 둔다.\n"
            "4. `RotationSign` scalar parameter를 추가한다. Binder가 `bClockwise ? +1 : -1`을 업로드한다.\n"
            "5. `AnvilWindDirectionXY` vector parameter와 `AnvilWindStretch`, `AnvilWindSkew`, `AnvilWarpStart01`, `AnvilWarpEnd01` scalar parameter를 추가한다.\n"
            "6. `LFNoiseWorldSize`, `HFNoiseWorldSize`, `CurlNoiseWorldSize` scalar parameter는 실제 텍스처의 km 단위 world size를 유지한다. Binder가 현재 storm radius를 이 값으로 나눠 세 `UnitsPerBodyRadius` parameter를 업로드한다.\n"
            "7. `WorldPosition`은 Absolute World Position을 사용한다. Coverage 또는 noise UV에서 반경/방향을 복원하는 노드는 제거한다.\n"
            "8. `SpiralCarveStrength`, `SpiralCarveCoverageFloor`, `FormationSwirlSign` 관련 핀·파라미터·주석은 삭제한다. `ShapeRT2G`라는 일반 이름도 `SpiralTendrilMask`로 교체한다.",
            "2. ShapeRT2의 R/B는 기존 height decode에 유지한다. G의 `SpiralTendrilMask`와 A의 `CenterMask`는 bake/debug 데이터로만 유지하고 `MF_Erosion`에는 연결하지 않는다.\n"
            "3. `RotationSign` scalar parameter를 유지한다. Binder가 `bClockwise ? +1 : -1`을 업로드한다.\n"
            "4. `AnvilWindDirectionXY` vector parameter와 `AnvilWindStretch`, `AnvilWindSkew`, `AnvilWarpStart01`, `AnvilWarpEnd01` scalar parameter를 유지한다.\n"
            "5. `LFNoiseWorldSize`, `HFNoiseWorldSize`, `CurlNoiseWorldSize` scalar parameter는 실제 텍스처의 km 단위 world size를 유지한다. Binder가 현재 storm radius를 이 값으로 나눠 세 `UnitsPerBodyRadius` parameter를 업로드한다.\n"
            "6. `WorldPosition`은 Absolute World Position을 사용한다. Coverage 또는 noise UV에서 반경/방향을 복원하는 노드는 제거한다.\n"
            "7. `SpiralTendrilMask`, `SpiralTendrilStrength`, `SpiralCarveStrength`, `SpiralCarveCoverageFloor`, `FormationSwirlSign` 관련 density 핀·파라미터·주석은 삭제한다.",
        ),
        (
            "Material Editor에서 순서대로 `Coverage`, `SpiralTendrilMask`, `UndersideBand`, `EffectiveBottomType`, `BodyProfile`, `FlippedAnvilProfile`, `AnvilWeight`, `HFFBM`, `LFShape`를 단독 출력해 확인한다. `SpiralTendrilStrength`를 0/1로 바꿔도 `Coverage`, upper band, `FlippedAnvilProfile`은 동일해야 한다.",
            "Material Editor에서 `Coverage`, `BaseBottomType`, `BodyProfile`, `FlippedAnvilProfile`, `AnvilWeight`, `HFFBM`, `LFShape`를 단독 출력해 확인한다. ShapeRT2.G를 변경해도 이 density/Wispy 출력은 동일해야 한다.",
        ),
    ),
)

print("Applied Tendril/Wispy source decoupling")
