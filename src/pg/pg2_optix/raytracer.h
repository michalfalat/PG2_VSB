#pragma once
#include "simpleguidx11.h"
#include "surface.h"

/*! \class Raytracer
\brief General ray tracer class.

\author Tomáš Fabián
\version 0.1
\date 2018
*/
class Raytracer : public SimpleGuiDX11
{
public:
	Raytracer( const int width, const int height );
	~Raytracer();

	int InitDeviceAndScene();
	int ReleaseDeviceAndScene();
	int InitGraph();
	int GetImage(BYTE * buffer) override;

	void LoadScene( const std::string file_name );
	
	//int GetImage( BYTE * buffer ) override;

	int Ui();

private:
	RTcontext context_{ 0 };
	RTbuffer output_buffer_{ 0 };
	std::vector<Surface *> surfaces_;
	std::vector<Material *> materials_;			

	bool unify_normals_{ true };
};
