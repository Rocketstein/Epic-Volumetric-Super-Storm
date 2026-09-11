$ErrorActionPreference = 'Stop'

function Restore-GitBlob
{
	param(
		[Parameter(Mandatory = $true)]
		[string] $GitObject,

		[Parameter(Mandatory = $true)]
		[string] $TargetPath
	)

	$startInfo = [System.Diagnostics.ProcessStartInfo]::new()
	$startInfo.FileName = 'git'
	$startInfo.UseShellExecute = $false
	$startInfo.RedirectStandardOutput = $true
	$startInfo.RedirectStandardError = $true
	$startInfo.ArgumentList.Add('show')
	$startInfo.ArgumentList.Add($GitObject)

	$process = [System.Diagnostics.Process]::new()
	$process.StartInfo = $startInfo
	if (-not $process.Start())
	{
		throw "Failed to start git show for $GitObject"
	}

	$fullTargetPath = [System.IO.Path]::GetFullPath($TargetPath)
	$fileStream = [System.IO.File]::Create($fullTargetPath)
	try
	{
		$process.StandardOutput.BaseStream.CopyTo($fileStream)
	}
	finally
	{
		$fileStream.Dispose()
	}

	$standardError = $process.StandardError.ReadToEnd()
	$process.WaitForExit()
	if ($process.ExitCode -ne 0)
	{
		throw "git show failed for ${GitObject}: $standardError"
	}
}

$workspaceRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))

Restore-GitBlob `
	'HEAD:VolumetricSuperStorm/Plugins/SavageSuperStorm/Source/SavageSuperStormRuntime/Public/Assets/StormVerticalProfileAsset.h' `
	(Join-Path $workspaceRoot 'Plugins\SavageSuperStorm\Source\SavageSuperStormRuntime\Public\Assets\StormVerticalProfileAsset.h')

Restore-GitBlob `
	'HEAD:VolumetricSuperStorm/Plugins/SavageSuperStorm/Source/SavageSuperStormEditor/Private/Widgets/SStormProfilePainter.cpp' `
	(Join-Path $workspaceRoot 'Plugins\SavageSuperStorm\Source\SavageSuperStormEditor\Private\Widgets\SStormProfilePainter.cpp')

Restore-GitBlob `
	'HEAD:VolumetricSuperStorm/Plugins/SavageSuperStorm/Source/SavageSuperStormEditor/Public/Widgets/SStormProfilePainter.h' `
	(Join-Path $workspaceRoot 'Plugins\SavageSuperStorm\Source\SavageSuperStormEditor\Public\Widgets\SStormProfilePainter.h')
