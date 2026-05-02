# END terminal shell integration for PowerShell - do not edit

# Source user profile if it exists
if (Test-Path $PROFILE) { . $PROFILE }

# Save original prompt
$_end_orig_prompt = $function:prompt

# Override prompt (precmd equivalent)
function prompt {
    $exitCode = $global:LASTEXITCODE
    # OSC 133 D - command finished
    $Host.UI.Write("`e]133;D;$exitCode`a")
    # OSC 7 - current working directory
    $Host.UI.Write("`e]7;file://$([System.Net.Dns]::GetHostName())/$($PWD.Path -replace '\\','/')`a")
    # OSC 133 A - prompt start
    $Host.UI.Write("`e]133;A`a")
    # Run original prompt
    $result = & $_end_orig_prompt
    # OSC 133 B - prompt end
    $Host.UI.Write("`e]133;B`a")
    $result
}

# Override Enter key (preexec equivalent)
if (Get-Module PSReadLine -ErrorAction SilentlyContinue) {
    Set-PSReadLineKeyHandler -Key Enter -ScriptBlock {
        # OSC 133 C - command start
        $Host.UI.Write("`e]133;C`a")
        [Microsoft.PowerShell.PSConsoleReadLine]::AcceptLine()
    }
}

function end {
    if ($args[0] -eq "preview") {
        $resolved = (Resolve-Path $args[1]).Path
        switch ($env:END_SKIT) {
            "kitty"  { [Console]::Write("`e_GEND;$resolved`e\") }
            "iterm2" { [Console]::Write("`e]1337;END;$resolved`a") }
            "sixel"  { [Console]::Write("`eP0qEND;$resolved`e\") }
        }
    } else {
        & $env:END_BINARY @args
    }
}
