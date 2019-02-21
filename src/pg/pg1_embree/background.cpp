#include "stdafx.h"
#include "Background.h"
#define _USE_MATH_DEFINES
#include <math.h>



Background::Background(const char filename[])
{
	texture = new Texture(filename);
}



Color4f Background::GetBackground(const float x, const float y, const float z) {
	const float theta = acosf(z);
	const float phi = atan2f(y, x) + float(M_PI);

	const float u = 1.0f - phi * 0.5f * float(M_1_PI);
	const float v = theta  * float(M_1_PI);

	return  texture->get_texel(u, v);
}


Background::~Background()
{
}
