#ifndef INCLUDE_MAGIC_KERNEL_H
#define INCLUDE_MAGIC_KERNEL_H
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct MagicKernelParams
{
	float *src;
	float *dst;
	int src_w;
	int src_h;
	int src_c;
	int dst_w;
	int dst_h;
	int dst_c;
	int from;
	int to;
};

double magic_kernel_sharp_2013 (double x);
void magic_kernel_resize (void *lpParameters);
#endif