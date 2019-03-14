#include "pch.h"
#include "raytracer.h"
#include "objloader.h"
#include "tutorials.h"
#include "mymath.h"
#include "omp.h"
#include "Camera.h"

Raytracer::Raytracer(const int width, const int height, const float fov_y, const Vector3 view_from, const Vector3 view_at) : SimpleGuiDX11(width, height)
{
	InitDeviceAndScene();
	camera = Camera(width, height, fov_y, view_from, view_at);
	fov = fov_y;
}


Raytracer::~Raytracer()
{
	ReleaseDeviceAndScene();
}

int Raytracer::InitDeviceAndScene()
{
	unsigned int version, count;
	{
		error_handler(rtGetVersion(&version));
		error_handler(rtDeviceGetDeviceCount(&count));
		const int major = version / 10000;
		const int minor = (version - major * 10000) / 100;
		const int micro = version - major * 10000 - minor * 100;
		printf("Nvidia OptiX %d.%d.%d, %d device(s) found\n", major, minor, micro, count);
	}

	/*int rtx_mode = 1;
	error_handler( rtGlobalSetAttribute( RT_GLOBAL_ATTRIBUTE_ENABLE_RTX, sizeof( rtx_mode ), &rtx_mode ) ); */

	for (unsigned int i = 0; i < count; ++i)
	{
		char name[64];
		error_handler(rtDeviceGetAttribute(i, RT_DEVICE_ATTRIBUTE_NAME, 64, name));
		RTsize memory_size = 0;
		error_handler(rtDeviceGetAttribute(i, RT_DEVICE_ATTRIBUTE_TOTAL_MEMORY, sizeof(RTsize), &memory_size));
		printf("%d : %s (%0.0f MB)\n", i, name, memory_size / (1024.0f * 1024.0f));
	}

	error_handler(rtContextCreate(&context_));
	error_handler(rtContextSetRayTypeCount(context_, 1));
	error_handler(rtContextSetEntryPointCount(context_, 1));
	error_handler(rtContextSetMaxTraceDepth(context_, 10));

	RTvariable output;
	error_handler(rtContextDeclareVariable(context_, "output_buffer", &output));
	// OptiX buffers are used to pass data between the host and the device
	error_handler(rtBufferCreate(context_, RT_BUFFER_OUTPUT, &output_buffer_));
	// before using a buffer, its size, dimensionality and element format must be specified
	error_handler(rtBufferSetFormat(output_buffer_, RT_FORMAT_UNSIGNED_BYTE4));
	error_handler(rtBufferSetSize2D(output_buffer_, width(), height()));
	// sets a program variable to an OptiX object value
	error_handler(rtVariableSetObject(output, output_buffer_));
	// host access to the data stored within a buffer is performed with the rtBufferMap function
	// all buffers must be unmapped via rtBufferUnmap before context validation will succeed
	return S_OK;
}

int Raytracer::InitGraph()
{
	RTprogram primary_ray;
	// https://devtalk.nvidia.com/default/topic/1043216/optix/how-to-generate-ptx-file-using-visual-studio/
	error_handler(rtProgramCreateFromPTXFile(context_, "optixtutorial.ptx", "primary_ray", &primary_ray));
	error_handler(rtContextSetRayGenerationProgram(context_, 0, primary_ray));
	error_handler(rtProgramValidate(primary_ray));

	RTprogram exception;
	error_handler(rtProgramCreateFromPTXFile(context_, "optixtutorial.ptx", "exception", &exception));
	error_handler(rtContextSetExceptionProgram(context_, 0, exception));
	error_handler(rtProgramValidate(exception));
	error_handler(rtContextSetExceptionEnabled(context_, RT_EXCEPTION_ALL, 1));

	error_handler(rtContextSetPrintEnabled(context_, 1));
	error_handler(rtContextSetPrintBufferSize(context_, 4096));

	RTprogram miss_program;
	error_handler(rtProgramCreateFromPTXFile(context_, "optixtutorial.ptx", "miss_program", &miss_program));
	error_handler(rtContextSetMissProgram(context_, 0, miss_program));
	error_handler(rtProgramValidate(miss_program));

	// RTgeometrytriangles type provides OptiX with built-in support for triangles	
	// geometry
	RTgeometrytriangles geometry_triangles;
	error_handler(rtGeometryTrianglesCreate(context_, &geometry_triangles));
	error_handler(rtGeometryTrianglesSetPrimitiveCount(geometry_triangles, 1));
	RTvariable normals;
	rtContextDeclareVariable(context_, "normal_buffer", &normals);
	RTbuffer normal_buffer;
	error_handler(rtBufferCreate(context_, RT_BUFFER_INPUT, &normal_buffer));
	error_handler(rtBufferSetFormat(normal_buffer, RT_FORMAT_FLOAT3));
	error_handler(rtBufferSetSize1D(normal_buffer, 3));
	{
		optix::float3 * data = nullptr;
		error_handler(rtBufferMap(normal_buffer, (void**)(&data)));
		data[0].x = 0.0f; data[0].y = 0.0f; data[0].z = 0.0f;
		data[1].x = 200.0f; data[1].y = 0.0f; data[1].z = 0.0f;
		data[2].x = 0.0f; data[2].y = 150.0f; data[2].z = 0.0f;
		error_handler(rtBufferUnmap(normal_buffer));
		data = nullptr;
	}

	rtBufferValidate(normal_buffer);
	rtVariableSetObject(normals, normal_buffer);
	error_handler(rtGeometryTrianglesSetVertices(geometry_triangles, 3, normal_buffer, 0, sizeof(optix::float3), RT_FORMAT_FLOAT3));
	//rtGeometryTrianglesSetTriangles();
	error_handler(rtGeometryTrianglesValidate(geometry_triangles));

	RTprogram attribute_program;
	rtProgramCreateFromPTXFile(context_, "optixtutorial.ptx", "attribute_program", &attribute_program);
	rtProgramValidate(attribute_program);
	rtGeometryTrianglesSetAttributeProgram(geometry_triangles, attribute_program);
	rtGeometryTrianglesValidate(geometry_triangles);

	// material
	RTmaterial material;
	error_handler(rtMaterialCreate(context_, &material));
	RTprogram closest_hit;
	error_handler(rtProgramCreateFromPTXFile(context_, "optixtutorial.ptx", "closest_hit", &closest_hit));
	error_handler(rtProgramValidate(closest_hit));
	error_handler(rtMaterialSetClosestHitProgram(material, 0, closest_hit));
	//rtMaterialSetAnyHitProgram( material, 0, any_hit );	
	error_handler(rtMaterialValidate(material));

	// geometry instance
	RTgeometryinstance geometry_instance;
	error_handler(rtGeometryInstanceCreate(context_, &geometry_instance));
	error_handler(rtGeometryInstanceSetGeometryTriangles(geometry_instance, geometry_triangles));
	error_handler(rtGeometryInstanceSetMaterialCount(geometry_instance, 1));
	error_handler(rtGeometryInstanceSetMaterial(geometry_instance, 0, material));
	error_handler(rtGeometryInstanceValidate(geometry_instance));
	// ---

	// acceleration structure
	RTacceleration sbvh;
	error_handler(rtAccelerationCreate(context_, &sbvh));
	error_handler(rtAccelerationSetBuilder(sbvh, "Sbvh"));
	//error_handler( rtAccelerationSetProperty( sbvh, "vertex_buffer_name", "vertex_buffer" ) );
	error_handler(rtAccelerationValidate(sbvh));

	// geometry group
	RTgeometrygroup geometry_group;
	error_handler(rtGeometryGroupCreate(context_, &geometry_group));
	error_handler(rtGeometryGroupSetAcceleration(geometry_group, sbvh));
	error_handler(rtGeometryGroupSetChildCount(geometry_group, 1));
	error_handler(rtGeometryGroupSetChild(geometry_group, 0, geometry_instance));
	error_handler(rtGeometryGroupValidate(geometry_group));

	RTvariable top_object;
	error_handler(rtContextDeclareVariable(context_, "top_object", &top_object));
	error_handler(rtVariableSetObject(top_object, geometry_group));


	RTvariable material_indices;
	rtContextDeclareVariable(context_, "material_index_buffer",
		&material_indices);
	RTbuffer material_index_buffer;
	rtBufferCreate(context_, RT_BUFFER_INPUT, &material_index_buffer
	);
	rtBufferSetFormat(material_index_buffer, RT_FORMAT_UNSIGNED_BYTE
	);
	rtBufferSetSize1D(material_index_buffer, 1); // to bbe used, must be larger than 1

	// group

	error_handler(rtContextValidate(context_));
	return S_OK;
}

int Raytracer::ReleaseDeviceAndScene()
{
	error_handler(rtContextDestroy(context_));
	return S_OK;
}

void Raytracer::LoadScene(const std::string file_name)
{
	const int no_surfaces = LoadOBJ(file_name.c_str(), surfaces_, materials_);

	InitGraph();

	// surfaces loop
	for (auto surface : surfaces_)
	{
		// triangles loop
		for (int i = 0, k = 0; i < surface->no_triangles(); ++i)
		{
			Triangle & triangle = surface->get_triangle(i);

			// vertices loop
			for (int j = 0; j < 3; ++j, ++k)
			{
				const Vertex & vertex = triangle.vertex(j);
			} // end of vertices loop

		} // end of triangles loop

	} // end of surfaces loop
}

int Raytracer::GetImage(BYTE * buffer)
{
	// call rtTrace and fill the buffer with the result

	error_handler(rtContextLaunch2D(context_, 0, width(), height()));

	optix::uchar4 * data = nullptr;
	error_handler(rtBufferMap(output_buffer_, (void**)(&data)));

	memcpy(buffer, data, width() * height() * sizeof(optix::uchar4));

	error_handler(rtBufferUnmap(output_buffer_));
	data = nullptr;

	return S_OK;
}

int Raytracer::Ui()
{
	static float f = 0.0f;
	static int counter = 0;

	// we use a Begin/End pair to created a named window
	ImGui::Begin("Ray Tracer Params");

	ImGui::Text("Surfaces = %d", surfaces_.size());
	ImGui::Text("Materials = %d", materials_.size());
	ImGui::Separator();
	ImGui::Checkbox("Vsync", &vsync_);
	ImGui::Checkbox("Unify normals", &unify_normals_);

	//ImGui::Combo( "Shader", &current_shader_, shaders_, IM_ARRAYSIZE( shaders_ ) );

	//ImGui::Checkbox( "Demo Window", &show_demo_window );      // Edit bools storing our window open/close state
	//ImGui::Checkbox( "Another Window", &show_another_window );

	ImGui::SliderFloat("gamma", &gamma_, 0.1f, 5.0f);            // Edit 1 float using a slider from 0.0f to 1.0f    
	//ImGui::ColorEdit3( "clear color", ( float* )&clear_color ); // Edit 3 floats representing a color

	if (ImGui::Button("Button"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
		counter++;
	ImGui::SameLine();
	ImGui::Text("counter = %d", counter);

	ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
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