// Magic Kernel Sharp 2013 algoritm, by John Costell, as implemented by Viddeleer (with minor adjusments).
#include "magic_kernel.h"

double magic_kernel_sharp_2013 (double x)
{
	if      (x < 0.0)  x = -x;
	if      (x <= 0.5) return 17.0 / 16.0 - (7.0 / 4.0) * x * x;
	else if (x <= 1.5) return 0.25 * (4.0 * x * x - 11.0 * x + 7.0);
	else if (x <= 2.5) return -0.125 * (x - 2.5) * (x - 2.5);
	else return 0.0;
}

void magic_kernel_resize (void *lpParameters)
{
	float* dst;
	float* src;
	int src_w, src_h, src_c, dst_w, dst_h, dst_c, from, to;

	struct MagicKernelParams inputparams;
	memcpy(&inputparams, lpParameters, sizeof(struct MagicKernelParams));
	src = (float*)inputparams.src;
	dst = (float*)inputparams.dst;
	src_w = inputparams.src_w;
	src_h = inputparams.src_h;
	src_c = inputparams.src_c;
	dst_w = inputparams.dst_w;
	dst_h = inputparams.dst_h;
	dst_c = inputparams.dst_c;
	from = inputparams.from;
	to = inputparams.to;

	if (!src || !dst) return;
	if (src_w <= 0 || src_h <= 0 || dst_w <= 0 || dst_h <= 0) return;

	// continuous ratios
	const double fx_ratio = (double)src_w / (double)dst_w;
	const double fy_ratio = (double)src_h / (double)dst_h;

	// scale used to compute integer footprint radius (>= 1)
	const double scale_x = (fx_ratio > 1.0) ? fx_ratio : 1.0;
	const double scale_y = (fy_ratio > 1.0) ? fy_ratio : 1.0;

	int radius_x = 3;
	int radius_y = 3;

	if (dst_w < src_w) radius_x = (int)ceil(2.5 * scale_x);
	if (dst_h < src_h) radius_y = (int)ceil(2.5 * scale_y);

	for (int dy = from; dy < to; ++dy)
	{
		// map dst pixel center to source continuous coordinate:
		double src_y_f = ((dy + 0.5) * (double)src_h / (double)dst_h) - 0.5;
		int iy = (int)floor(src_y_f);
		double frac_y = src_y_f - (double)iy;   // in [0,1)

		for (int dx = 0; dx < dst_w; ++dx)
		{
			double src_x_f = ((dx + 0.5) * (double)src_w / (double)dst_w) - 0.5;
			int ix = (int)floor(src_x_f);
			double frac_x = src_x_f - (double)ix;

			double sum_r = 0.0;
			double sum_g = 0.0;
			double sum_b = 0.0;
			double sum_a = 0.0;
			double wsum = 0.0;

			//int isedge = 0;

			// iterate over contributing source pixels
			for (int ky = -radius_y; ky <= radius_y; ++ky)
			{
				int sy = iy + ky;
				if ((unsigned)sy >= (unsigned)src_h) { /*isedge = 1; */continue; }

				// distance in Y between sample pos and that row
				double dy_dist = fabs(frac_y - (double)ky) / scale_y;
				if (dy_dist >= 2.5) continue;
				double wy = magic_kernel_sharp_2013(dy_dist);

				for (int kx = -radius_x; kx <= radius_x; ++kx)
				{
					int sx = ix + kx;
					if ((unsigned)sx >= (unsigned)src_w) {/* isedge = 1;*/ continue; }

					double dx_dist = fabs(frac_x - (double)kx) / scale_x;
					if (dx_dist >= 2.5) continue;
					double wx = magic_kernel_sharp_2013(dx_dist);

					double w = wx * wy;

					int pixel_index = (sy * src_w + sx) * src_c;

					// accumulate as double for better precision
					/*if (w < 0)
					  printf("w = %f  wx = %f  wy = %f\n", w, wx, wy);*/
					sum_r += src[pixel_index++] * w;
					sum_g += src[pixel_index++] * w;
					sum_b += src[pixel_index++] * w;
					if (src_c >= 4) sum_a += src[pixel_index] * w;
					wsum += w;
				}
			}

			int idx = (dy * dst_w + dx) * dst_c;

			//if (!isedge) wsum = 1.0;			// for upscaling no normalization is needed

			if (wsum > 0.0)
			{ // normalize
				wsum = 1.0 / wsum;
				// Why are the sums sometimes negative?
        if (sum_r >= 0.0)
				  dst[idx] = (float)(sum_r * wsum);
				idx++;
				if (sum_g >= 0.0)
				  dst[idx] = (float)(sum_g * wsum);
				idx++;
			  if (sum_b >= 0.0)
			    dst[idx] = (float)(sum_b * wsum);
			  idx++;
				if (dst_c >= 4)
				  dst[idx] = (float)(sum_a * wsum);
			}
			else
			{ // no contributors
				dst[idx++] = 0;
				dst[idx++] = 0;
				dst[idx++] = 0;
				if (dst_c >= 4)
				  dst[idx] = 0;
			}
		}
	}
}