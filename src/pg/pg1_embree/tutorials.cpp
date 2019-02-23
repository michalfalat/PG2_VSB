#include "stdafx.h"
#include "tutorials.h"
#include "raytracer.h"
#include "structs.h"
#include "texture.h"
#include "mymath.h"
#include "Background.h"
#include "curand.h"

/* error reporting function */
void error_handler(void * user_ptr, const RTCError code, const char * str)
{
	if (code != RTC_ERROR_NONE)
	{
		std::string descr = str ? ": " + std::string(str) : "";

		switch (code)
		{
		case RTC_ERROR_UNKNOWN: throw std::runtime_error("RTC_ERROR_UNKNOWN" + descr);
		case RTC_ERROR_INVALID_ARGUMENT: throw std::runtime_error("RTC_ERROR_INVALID_ARGUMENT" + descr); break;
		case RTC_ERROR_INVALID_OPERATION: throw std::runtime_error("RTC_ERROR_INVALID_OPERATION" + descr); break;
		case RTC_ERROR_OUT_OF_MEMORY: throw std::runtime_error("RTC_ERROR_OUT_OF_MEMORY" + descr); break;
		case RTC_ERROR_UNSUPPORTED_CPU: throw std::runtime_error("RTC_ERROR_UNSUPPORTED_CPU" + descr); break;
		case RTC_ERROR_CANCELLED: throw std::runtime_error("RTC_ERROR_CANCELLED" + descr); break;
		default: throw std::runtime_error("invalid error code" + descr); break;
		}
	}
}

/* adds a single triangle to the scene */
unsigned int add_triangle(const RTCDevice device, RTCScene scene)
{
	// geometries are objects that represent an array of primitives of the same type, so lets create a triangle
	RTCGeometry mesh = rtcNewGeometry(device, RTC_GEOMETRY_TYPE_TRIANGLE);

	// and depending on the geometry type, different buffers must be bound (typically, vertex and index buffer is required)

	// set vertices in the newly created buffer
	Vertex3f * vertices = (Vertex3f *)rtcSetNewGeometryBuffer(
		mesh, RTC_BUFFER_TYPE_VERTEX, 0, RTC_FORMAT_FLOAT3, sizeof(Vertex3f), 3);
	vertices[0].x = 0; vertices[0].y = 0; vertices[0].z = 0;
	vertices[1].x = 2; vertices[1].y = 0; vertices[1].z = 0;
	vertices[2].x = 0; vertices[2].y = 3; vertices[2].z = 0;

	// set triangle indices
	Triangle3ui * triangles = (Triangle3ui *)rtcSetNewGeometryBuffer(mesh, RTC_BUFFER_TYPE_INDEX, 0, RTC_FORMAT_UINT3, sizeof(Triangle3ui), 1);
	triangles[0].v0 = 0; triangles[0].v1 = 1; triangles[0].v2 = 2;

	// see also rtcSetSharedGeometryBuffer, rtcSetGeometryBuffer		

	/*
	The parametrization of a triangle uses the first vertex p0 as base point, the vector (p1 - p0) as u-direction and the vector (p2 - p0) as v-direction.
	Thus vertex attributes t0, t1, t2 can be linearly interpolated over the triangle the following way:

	t_uv = (1-u-v)*t0 + u*t1 + v*t2	= t0 + u*(t1-t0) + v*(t2-t0)
	*/

	// sets the number of slots (vertexAttributeCount parameter) for vertex attribute buffers (RTC_BUFFER_TYPE_VERTEX_ATTRIBUTE)
	rtcSetGeometryVertexAttributeCount(mesh, 2);

	// set vertex normals
	Normal3f * normals = (Normal3f *)rtcSetNewGeometryBuffer(mesh, RTC_BUFFER_TYPE_VERTEX_ATTRIBUTE, 0, RTC_FORMAT_FLOAT3, sizeof(Normal3f), 3);
	normals[0].x = 0; normals[0].y = 0; normals[0].z = 1;
	normals[1].x = 0; normals[1].y = 0; normals[1].z = 1;
	normals[2].x = 0; normals[2].y = 0; normals[2].z = 1;

	// set texture coordinates
	Coord2f * tex_coords = (Coord2f *)rtcSetNewGeometryBuffer(mesh, RTC_BUFFER_TYPE_VERTEX_ATTRIBUTE, 1, RTC_FORMAT_FLOAT2, sizeof(Coord2f), 3);
	tex_coords[0].u = 0; tex_coords[0].v = 1;
	tex_coords[1].u = 1; tex_coords[1].v = 1;
	tex_coords[2].u = 0; tex_coords[2].v = 0;

	// changes to the geometry must be always committed
	rtcCommitGeometry(mesh);

	// geometries can be attached to a single scene
	unsigned int geom_id = rtcAttachGeometry(scene, mesh);
	// release geometry handle
	rtcReleaseGeometry(mesh);

	return geom_id;
}

/* generate a single ray and get the closest intersection with the scene */
int generate_and_trace_ray(RTCScene & scene)
{
	// setup a primary ray
	RTCRay ray;
	ray.org_x = 0.1f; // ray origin
	ray.org_y = 0.2f;
	ray.org_z = 2.0f;
	ray.tnear = FLT_MIN; // start of ray segment

	ray.dir_x = 0.0f; // ray direction
	ray.dir_y = 0.0f;
	ray.dir_z = -1.0f;
	ray.time = 0.0f; // time of this ray for motion blur

	ray.tfar = FLT_MAX; // end of ray segment (set to hit distance)

	ray.mask = 0; // can be used to mask out some geometries for some rays
	ray.id = 0; // identify a ray inside a callback function
	ray.flags = 0; // reserved

	// setup a hit
	RTCHit hit;
	hit.geomID = RTC_INVALID_GEOMETRY_ID;
	hit.primID = RTC_INVALID_GEOMETRY_ID;
	hit.Ng_x = 0.0f; // geometry normal
	hit.Ng_y = 0.0f;
	hit.Ng_z = 0.0f;

	// merge ray and hit structures
	RTCRayHit ray_hit;
	ray_hit.ray = ray;
	ray_hit.hit = hit;

	// intersect ray with the scene
	RTCIntersectContext context;
	rtcInitIntersectContext(&context);
	rtcIntersect1(scene, &context, &ray_hit);

	if (ray_hit.hit.geomID != RTC_INVALID_GEOMETRY_ID)
	{
		// we hit something
		RTCGeometry geometry = rtcGetGeometry(scene, ray_hit.hit.geomID);
		Normal3f normal;
		// get interpolated normal
		rtcInterpolate0(geometry, ray_hit.hit.primID, ray_hit.hit.u, ray_hit.hit.v,
			RTC_BUFFER_TYPE_VERTEX_ATTRIBUTE, 0, &normal.x, 3);
		// and texture coordinates
		Coord2f tex_coord;
		rtcInterpolate0(geometry, ray_hit.hit.primID, ray_hit.hit.u, ray_hit.hit.v,
			RTC_BUFFER_TYPE_VERTEX_ATTRIBUTE, 1, &tex_coord.u, 2);

		printf("normal = (%0.3f, %0.3f, %0.3f)\n", normal.x, normal.y, normal.z);
		printf("tex_coord = (%0.3f, %0.3f)\n", tex_coord.u, tex_coord.v);
	}

	return EXIT_SUCCESS;
}

/* simple tutorial how to find the closest intersection with a single triangle using embree */
int tutorial_1(const char * config)
{
	RTCDevice device = rtcNewDevice(config);
	error_handler(nullptr, rtcGetDeviceError(device), "Unable to create a new device.\n");
	rtcSetDeviceErrorFunction(device, error_handler, nullptr);

	ssize_t triangle_supported = rtcGetDeviceProperty(device, RTC_DEVICE_PROPERTY_TRIANGLE_GEOMETRY_SUPPORTED);

	// create a new scene bound to the specified device
	RTCScene scene = rtcNewScene(device);

	// add a single triangle geometry to the scene
	unsigned int triangle_geom_id = add_triangle(device, scene);

	// commit changes to scene
	rtcCommitScene(scene);

	generate_and_trace_ray(scene);

	// release scene and detach with all geometries
	rtcReleaseScene(scene);

	rtcReleaseDevice(device);

	return EXIT_SUCCESS;
}

/* texture loading and texel access */
int tutorial_2()
{
	// create texture
	Texture texture("../../../data/test4.png");
	Color4f texel = texture.get_texel((1.0f / texture.width()) * 2.5f, 0.0f);
	printf("(r = %0.3f, g = %0.3f, b = %0.3f)\n", texel.r, texel.g, texel.b);

	return EXIT_SUCCESS;
}

/* raytracer mainloop */
int tutorial_3(const std::string file_name, const char * config)
{
	//SimpleGuiDX11 gui( 640, 480 );
	//gui.MainLoop();


	// GEOSPHERE
	//Raytracer raytracer(640, 480, deg2rad(45.0),
	//	Vector3(3, 0, 0), Vector3(0, 0, 0), config);

	//Ship Model
	Raytracer raytracer(640, 480, deg2rad(50.0),
		Vector3(175, -140, 130), Vector3(0, 0, 35), config);

	//PATH Tracer
	/*Raytracer raytracer(640, 480, deg2rad(40.0),
		Vector3(40, -940, 250), Vector3(0, 0, 250), config);*/
	raytracer.LoadScene(file_name);
	raytracer.MainLoop();

	return EXIT_SUCCESS;
}

int ship_model(const std::string file_name, const char * config)
{

	//Ship Model
	Raytracer raytracer(640, 480, deg2rad(50.0),
		Vector3(175, -140, 130), Vector3(0, 0, 35), config);

	raytracer.LoadScene(file_name);
	raytracer.MainLoop();

	return EXIT_SUCCESS;
}

int path_tracer(const std::string file_name, const char * config)
{

	//Ship Model
	Raytracer raytracer(640, 480, deg2rad(40.0),
		Vector3(40, -940, 250), Vector3(0, 0, 250), config);

	raytracer.LoadScene(file_name);
	raytracer.MainLoop();

	return EXIT_SUCCESS;
}

int geosphere(const std::string file_name, const char * config)
{

	//Geosphere
	Raytracer raytracer(640, 480, deg2rad(45.0),
		Vector3(3, 0, 0), Vector3(0, 0, 0), config);

	raytracer.LoadScene(file_name);
	raytracer.MainLoop();

	return EXIT_SUCCESS;
}

/* OptiX error reporting function */
void error_handler(RTresult code)
{
	if (code != RT_SUCCESS)
	{
		throw std::runtime_error("RT_ERROR_UNKNOWN");
	}
}

int tutorial_7()
{
	int width = 640;
	int height = 480;

	unsigned int version, count;
	{
		error_handler(rtGetVersion(&version));
		error_handler(rtDeviceGetDeviceCount(&count));
		const int major = version / 10000;
		const int minor = (version - major * 10000) / 100;
		const int micro = version - major * 10000 - minor * 100;
		printf("NVIDIA OptiX %d.%d.%d, %d device(s) found\n", major, minor, micro, count);
	}

	/*int rtx_mode = 1;
	error_handler( rtGlobalSetAttribute( RT_GLOBAL_ATTRIBUTE_ENABLE_RTX, sizeof( rtx_mode ), &rtx_mode ) ); */

	for (unsigned int i = 0; i < count; ++i)
	{
		char name[64];
		error_handler(rtDeviceGetAttribute(i, RT_DEVICE_ATTRIBUTE_NAME, 64, name));
		RTsize memory_size = 0;
		error_handler(rtDeviceGetAttribute(i, RT_DEVICE_ATTRIBUTE_TOTAL_MEMORY, sizeof(RTsize), &memory_size));
		printf("%d : %s (%0.0f MB)\n", i, name, memory_size / sqr(1024.0f));
	}

	RTcontext context = 0;
	error_handler(rtContextCreate(&context));
	error_handler(rtContextSetRayTypeCount(context, 1));
	error_handler(rtContextSetEntryPointCount(context, 1));
	error_handler(rtContextSetMaxTraceDepth(context, 10));

	RTvariable output;
	error_handler(rtContextDeclareVariable(context, "output_buffer", &output));
	// OptiX buffers are used to pass data between the host and the device
	RTbuffer output_buffer;
	error_handler(rtBufferCreate(context, RT_BUFFER_OUTPUT, &output_buffer));
	// before using a buffer, its size, dimensionality and element format must be specified
	error_handler(rtBufferSetFormat(output_buffer, RT_FORMAT_UNSIGNED_BYTE4));
	error_handler(rtBufferSetSize2D(output_buffer, width, height));
	// sets a program variable to an OptiX object value
	error_handler(rtVariableSetObject(output, output_buffer));
	// host access to the data stored within a buffer is performed with the rtBufferMap function
	// all buffers must be unmapped via rtBufferUnmap before context validation will succeed


	RTprogram primary_ray;
	// https://devtalk.nvidia.com/default/topic/1043216/optix/how-to-generate-ptx-file-using-visual-studio/
	error_handler(rtProgramCreateFromPTXFile(context, "optixtutorial.ptx", "primary_ray", &primary_ray));
	error_handler(rtContextSetRayGenerationProgram(context, 0, primary_ray));
	error_handler(rtProgramValidate(primary_ray));

	RTprogram exception;
	error_handler(rtProgramCreateFromPTXFile(context, "optixtutorial.ptx", "exception", &exception));
	error_handler(rtContextSetExceptionProgram(context, 0, exception));
	error_handler(rtProgramValidate(exception));
	error_handler(rtContextSetExceptionEnabled(context, RT_EXCEPTION_ALL, 1));

	error_handler(rtContextSetPrintEnabled(context, 1));
	error_handler(rtContextSetPrintBufferSize(context, 4096));

	RTprogram miss_program;
	error_handler(rtProgramCreateFromPTXFile(context, "optixtutorial.ptx", "miss_program", &miss_program));
	error_handler(rtContextSetMissProgram(context, 0, miss_program));
	error_handler(rtProgramValidate(miss_program));

	// ---	
	// RTgeometrytriangles type provides OptiX with built-in support for triangles

	//RTgeometrytriangles
	//rtGeometryTrianglesSetTriangles(
	//rtGeometryTrianglesCreate(
	//rtGeometryInstanceSetGeometryTriangles( RTgeometryinstance instance, RTgeometrytriangles geometry );
	// geometry
	RTgeometrytriangles geometry_triangles;
	error_handler(rtGeometryTrianglesCreate(context, &geometry_triangles));
	error_handler(rtGeometryTrianglesSetPrimitiveCount(geometry_triangles, 1));
	RTbuffer vertex_buffer;
	error_handler(rtBufferCreate(context, RT_BUFFER_INPUT, &vertex_buffer));
	error_handler(rtBufferSetFormat(vertex_buffer, RT_FORMAT_FLOAT3));
	error_handler(rtBufferSetSize1D(vertex_buffer, 3));
	{
		float3 * data = nullptr;
		error_handler(rtBufferMap(vertex_buffer, (void**)(&data)));
		data[0].x = 0.0f; data[0].y = 0.0f; data[0].z = 0.0f;
		data[1].x = 200.0f; data[1].y = 0.0f; data[1].z = 0.0f;
		data[2].x = 0.0f; data[2].y = 150.0f; data[2].z = 0.0f;
		error_handler(rtBufferUnmap(vertex_buffer));
		data = nullptr;
	}
	error_handler(rtGeometryTrianglesSetVertices(geometry_triangles, 3, vertex_buffer, 0, sizeof(float3), RT_FORMAT_FLOAT3));
	//rtGeometryTrianglesSetTriangles();
	error_handler(rtGeometryTrianglesValidate(geometry_triangles));

	// material
	RTmaterial material;
	error_handler(rtMaterialCreate(context, &material));
	RTprogram closest_hit;
	error_handler(rtProgramCreateFromPTXFile(context, "optixtutorial.ptx", "closest_hit", &closest_hit));
	error_handler(rtProgramValidate(closest_hit));
	error_handler(rtMaterialSetClosestHitProgram(material, 0, closest_hit));
	//rtMaterialSetAnyHitProgram( material, 0, any_hit );	
	error_handler(rtMaterialValidate(material));

	// geometry instance
	RTgeometryinstance geometry_instance;
	error_handler(rtGeometryInstanceCreate(context, &geometry_instance));
	error_handler(rtGeometryInstanceSetGeometryTriangles(geometry_instance, geometry_triangles));
	error_handler(rtGeometryInstanceSetMaterialCount(geometry_instance, 1));
	error_handler(rtGeometryInstanceSetMaterial(geometry_instance, 0, material));
	error_handler(rtGeometryInstanceValidate(geometry_instance));
	// ---

	// acceleration structure
	RTacceleration sbvh;
	error_handler(rtAccelerationCreate(context, &sbvh));
	error_handler(rtAccelerationSetBuilder(sbvh, "Sbvh"));
	//error_handler( rtAccelerationSetProperty( sbvh, "vertex_buffer_name", "vertex_buffer" ) );
	error_handler(rtAccelerationValidate(sbvh));

	// geometry group
	RTgeometrygroup geometry_group;
	error_handler(rtGeometryGroupCreate(context, &geometry_group));
	error_handler(rtGeometryGroupSetAcceleration(geometry_group, sbvh));
	error_handler(rtGeometryGroupSetChildCount(geometry_group, 1));
	error_handler(rtGeometryGroupSetChild(geometry_group, 0, geometry_instance));
	error_handler(rtGeometryGroupValidate(geometry_group));

	RTvariable top_object;
	error_handler(rtContextDeclareVariable(context, "top_object", &top_object));
	error_handler(rtVariableSetObject(top_object, geometry_group));

	// group ???

	error_handler(rtContextValidate(context));
	error_handler(rtContextLaunch2D(context, 0, width, height));

	uchar4 * data = nullptr;
	error_handler(rtBufferMap(output_buffer, (void**)(&data)));
	FILE * file = fopen("output.ppm", "wt");
	fprintf(file, "P3\n%d %d\n255\n", width, height);
	for (int y = 0, z = 0; y < height; ++y)
	{
		for (int x = 0; x < width; ++x)
		{
			fprintf(file, "%03d %03d %03d\t", data[x + y * width].x, data[x + y * width].y, data[x + y * width].z);
			if (z++ == 4)
			{
				z = 0;
				fprintf(file, "\n");
			}
		}
	}
	fclose(file);
	file = nullptr;
	printf("%d %d %d %d\n", data[0].x, data[0].y, data[0].z, data[0].w);
	error_handler(rtBufferUnmap(output_buffer));
	data = nullptr;

	error_handler(rtGeometryTrianglesDestroy(geometry_triangles));
	error_handler(rtBufferDestroy(output_buffer));
	error_handler(rtProgramDestroy(primary_ray));

	error_handler(rtContextDestroy(context));

	return EXIT_SUCCESS;
}
