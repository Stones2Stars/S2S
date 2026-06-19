Add-Type -AssemblyName System.Web
$escaped_path = [System.Web.HttpUtility]::HtmlEncode($env:BTS_DIR)
(Get-Content '..\Sources\S2S.vcxproj.user.template') -replace '{BTSDIR}', $escaped_path | Out-File -encoding ASCII '..\Sources\S2S.vcxproj.user'