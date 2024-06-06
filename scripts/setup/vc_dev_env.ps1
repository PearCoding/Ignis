$Arch = "amd64"

# A simplified version of the Launch.VsDevShell.ps1
function LaunchDevShell {
    $VsWherePath = "`"${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe`""    
    $config = Invoke-Expression "& $VsWherePath -latest -format json" | ConvertFrom-Json
    $basePath = $config.installationPath
    $instanceId = $config.instanceId

    $currModulePath = "$basePath\Common7\Tools\Microsoft.VisualStudio.DevShell.dll"
    # Prior to 16.3 the DevShell module was in a different location
    $prevModulePath = "$basePath\Common7\Tools\vsdevshell\Microsoft.VisualStudio.DevShell.dll"

    $modulePath = if (Test-Path $prevModulePath) { $prevModulePath } else { $currModulePath }

    if (Test-Path $modulePath) {
        try {
            Import-Module $modulePath
        }
        catch [System.IO.FileLoadException] {
            Write-Verbose "The module has already been imported from a different installation of Visual Studio:"
            (Get-Module Microsoft.VisualStudio.DevShell).Path | Write-Verbose
        }

        $params = @{
            VsInstanceId = $instanceId
        }

        $command = Get-Command Enter-VsDevShell

        # -Arch is only available from 17.1
        if ($Arch -and $command.Parameters.ContainsKey("Arch")) {
            $params.Arch = $Arch
        }

        $params.SkipAutomaticLocation = $true
        Enter-VsDevShell @params
        exit
    }

    throw [System.Management.Automation.ErrorRecord]::new(
        [System.Exception]::new("Required assembly could not be located. This most likely indicates an installation error. Try repairing your Visual Studio installation. Expected location: $modulePath"),
        "DevShellModuleLoad",
        [System.Management.Automation.ErrorCategory]::NotInstalled,
        $config)
}

LaunchDevShell
