#include "pch.h"
#include "raytracer.h"
#include "objloader.h"
#include "tutorials.h"
#include "mymath.h"
#include "omp.h"

Raytracer::Raytracer( const int width, const int height ) : SimpleGuiDX11( width, height )
{
	InitDeviceAndScene();
}

Raytracer::~Raytracer()
{
	ReleaseDeviceAndScene();
}

int Raytracer::InitDeviceAndScene()
{	
	return S_OK;
}

int Raytracer::ReleaseDeviceAndScene()
{	
	return S_OK;
}

void Raytracer::LoadScene( const std::string file_name )
{
	const int no_surfaces = LoadOBJ( file_name.c_str(), surfaces_, materials_ );

	// surfaces loop
	for ( auto surface : surfaces_ )
	{		
		// triangles loop
		for ( int i = 0, k = 0; i < surface->no_triangles(); ++i )
		{
			Triangle & triangle = surface->get_triangle( i );

			// vertices loop
			for ( int j = 0; j < 3; ++j, ++k )
			{
				const Vertex & vertex = triangle.vertex( j );		
			} // end of vertices loop

		} // end of triangles loop

	} // end of surfaces loop
}

/*int Raytracer::GetImage( BYTE * buffer )
{
	// call rtTrace and fill the buffer with the result

	return S_OK;
}*/

int Raytracer::Ui()
{
	static float f = 0.0f;
	static int counter = 0;

	// we use a Begin/End pair to created a named window
	ImGui::Begin( "Ray Tracer Params" );
	
	ImGui::Text( "Surfaces = %d", surfaces_.size() );
	ImGui::Text( "Materials = %d", materials_.size() );
	ImGui::Separator();
	ImGui::Checkbox( "Vsync", &vsync_ );
	ImGui::Checkbox( "Unify normals", &unify_normals_ );	
	
	//ImGui::Combo( "Shader", &current_shader_, shaders_, IM_ARRAYSIZE( shaders_ ) );
	
	//ImGui::Checkbox( "Demo Window", &show_demo_window );      // Edit bools storing our window open/close state
	//ImGui::Checkbox( "Another Window", &show_another_window );

	ImGui::SliderFloat( "gamma", &gamma_, 0.1f, 5.0f );            // Edit 1 float using a slider from 0.0f to 1.0f    
	//ImGui::ColorEdit3( "clear color", ( float* )&clear_color ); // Edit 3 floats representing a color

	if ( ImGui::Button( "Button" ) )                            // Buttons return true when clicked (most widgets return true when edited/activated)
		counter++;
	ImGui::SameLine();
	ImGui::Text( "counter = %d", counter );

	ImGui::Text( "Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate );
	ImGui::End();

	// 3. Show another simple window.
	/*if ( show_another_window )
	{
	ImGui::Begin( "Another Window", &show_another_window );   // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
	ImGui::Text( "Hello from another window!" );
	if ( ImGui::Button( "Close Me" ) )
	show_another_window = false;
	ImGui::End();
	}*/

	return 0;
}
