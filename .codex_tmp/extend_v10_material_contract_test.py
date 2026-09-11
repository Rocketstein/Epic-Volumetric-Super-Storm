from pathlib import Path


path = Path("Plugins/SavageSuperStorm/Source/SavageSuperStormRuntime/Private/Tests/StormDensityContractTests.cpp")
text = path.read_text(encoding="utf-8")

old_include = '#include "Materials/Material.h"\n'
new_include = '#include "Materials/Material.h"\n#include "Material/StormCloudMaterialParameters.h"\n'
if text.count(old_include) != 1:
    raise RuntimeError("Materials/Material include anchor changed")
text = text.replace(old_include, new_include)

old_assert = '''    TestNotNull(TEXT("M_SSS has a completed game-thread shader map"),
        Resource->GetGameThreadShaderMap());
    return CompileErrors.IsEmpty();
'''
new_assert = '''    TestNotNull(TEXT("M_SSS has a completed game-thread shader map"),
        Resource->GetGameThreadShaderMap());
    TestTrue(TEXT("M_SSS exposes the complete v10 runtime parameter contract"),
        CloudMaterialParams::HasUnifiedFieldMaterialContract(Material));
    return CompileErrors.IsEmpty();
'''
if text.count(old_assert) != 1:
    raise RuntimeError("MSSSCompile assertion anchor changed")
text = text.replace(old_assert, new_assert)
path.write_text(text, encoding="utf-8")
