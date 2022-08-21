using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using static System.Net.Mime.MediaTypeNames;
using System.Drawing;

namespace aperture_renderer
{
	public struct Aperture
	{
		public float[,] IntensityMask;
		public float[,] ZValues;

		public int Width;
		public int Height;

		public float Velocity;
		public float D;
		public float Lambda;

		// R is the radius of the 'lense', with the centre at (Width/2.0, Height/2.0, R), it affects the 
		// curvature of 'ZValues'
		public Aperture(Bitmap bmp, float d, float R, float lambda)
		{
			IntensityMask = new float[bmp.Width, bmp.Height];

			ZValues = new float[bmp.Width, bmp.Height];

			Width = bmp.Width;
			Height = bmp.Height;

			float cx = bmp.Width / 2.0f;
			float cy = bmp.Height / 2.0f;

			D = d;

			Lambda = lambda;

			Velocity = (float)(lambda / 2.0 / Math.PI);
			
			// Loop through the images pixels to reset color.
			for (int x = 0; x < bmp.Width; x++)
			{
				for (int y = 0; y < bmp.Height; y++)
				{
					Color px = bmp.GetPixel(x, y);

					IntensityMask[x, y] = (px.R + px.G + px.B) / 3.0f / 255.0f;

					ZValues[x, y] = (float)-Math.Sqrt(R * R - Math.Pow(x - cx, 2.0) - Math.Pow(y - cy, 2.0)) + d;
				}
			}
		}

		public float DiffValue(int x, int y)
		{
			float accum_a = 0;
			float accum_b = 0;

			for (int ay = 0; ay < Height; ++ay)
			{
				for (int ax = 0; ax < Width; ++ax)
				{
					float i = IntensityMask[ax, ay];
					if (i < 0.00001)
						continue;

					float z = ZValues[ax, ay];

					var l_sqr = Math.Pow(ax - x, 2) + Math.Pow(ay - y, 2) + Math.Pow(z - D, 2);
					//var l_sqr = PowX[ax - x + Width] + PowY[ay - y + Height] + Math.Pow(z - D, 2);
					var d_t = Math.Sqrt(l_sqr) / Velocity;

					float ivq = (float)(IntensityMask[ax, ay] / l_sqr);
					//float ivq = 1.0f;

					accum_a += (float)(ivq * Math.Cos(d_t));
					accum_b += (float)(ivq * Math.Sin(d_t));
				}
			}

			return (float)(Math.PI * (Math.Pow(accum_a, 2.0) + Math.Pow(accum_b, 2.0)));
		}
	}
}
