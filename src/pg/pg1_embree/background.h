#pragma once
#include "texture.h"
class Background
{
public:
	Background() : texture(texture) {};
	Background(const char filename[]) ;
	Color4f GetBackground(const float x, const float y, const float z);
	~Background();
private:
	Texture *texture;
};


