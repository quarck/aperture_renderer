

$exe = "x64\Release\aperture_renderer.exe"
$apertures = (gci sample_apertures)

$wavelengths = (0.35, 0.55, 0.75, 1.05)

$d = 200
$R = 1000

foreach ($ap in $apertures)
{
	foreach ($wv in $wavelengths)
	{
		$n = ([string]$wv) -replace '\.',''		
		$out_file = $ap -replace '\.png', ("-"+$n+".png")
		
		Write-Host "Runniing $ap for $wv nm"
		
		& $exe sample_apertures\$ap out\$out_file $d $R $wv		
		
		Write-Host "Done Runniing $ap for $wv nm"
	}
}
