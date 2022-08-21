using System.Drawing;

// You can define other methods, fields, classes and namespaces here

// See https://aka.ms/new-console-template for more information
// ^^ This is terrible... 

string file = @"C:\Users\sergey\OneDrive\src\python\Notebooks\difraction\small_hex.png";
var img = Image.FromFile(file);
var bmp = new Bitmap(img);

var ap = new aperture_renderer.Aperture(bmp, 200.0f, 1000.0f, 0.5f);

var outimg = (Bitmap)bmp.Clone();

var max = ap.DiffValue(bmp.Width / 2 - 4, bmp.Height / 2 - 4);

//Parallel.For(0, outimg.Width, (x) =>
//{
//	Console.WriteLine(x);
//	for (int y = 0; y < outimg.Height; y++)
//	{
//		var dv = ap.DiffValue(x, y) / max;
//		int pv = Math.Min(255, (int)(255 * dv));
//		outimg.SetPixel(x, y, Color.FromArgb(pv, pv, pv));
//	}
//});


Parallel.For(0, outimg.Height, (y) => 
{
	Console.WriteLine(y);

	for (int x = 0; x < outimg.Width; x++)
	{
		var dv = ap.DiffValue(x, y) / max;
		int pv = Math.Min(255, (int)(255 * dv));
		lock (outimg)
			outimg.SetPixel(x, y, Color.FromArgb(pv, pv, pv));
	}
});

//for (int y = 0; y < outimg.Height; y++)
//{
//	Console.WriteLine(y);

//	for (int x = 0; x < outimg.Width; x++)
//	{
//		var dv = ap.DiffValue(x, y) / max;
//		int pv = Math.Min(255, (int)(255 * dv));
//		outimg.SetPixel(x, y, Color.FromArgb(pv, pv, pv));
//	}
//}

outimg.Save(@"C:\Users\sergey\OneDrive\src\python\Notebooks\difraction\small_hex_out2.png");
