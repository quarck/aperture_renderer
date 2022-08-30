

$exe = "x64\Release\aperture_renderer.exe"
$apertures = (gci sample_apertures)

$R = 1000
$wl = 0.75

foreach ($ap in $apertures)
{
	$out_file = "sample_results\" + $ap.Name 

    if (Test-Path $out_file)
    {
        Write-Host "Skipping" $ap.Name 
        continue
    }
		
	Write-Host "Runniing $ap"
		
	& $exe sample_apertures\$ap $out_file $R $wv
		
	Write-Host "Done $ap"
}
