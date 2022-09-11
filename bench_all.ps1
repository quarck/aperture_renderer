

$exe = "x64\Release\aperture_renderer.exe"
$apertures = (gci bench)

foreach ($ap in $apertures)
{
	if ($ap.Name.Contains( "out.png"))
	{
		continue
	}

	$out_file = $ap -replace '\.png', ("-out.png")

	echo $ap.Name $out_file	
	
	& $exe bench\$ap bench\$out_file 1000 0.75 1.0
}
