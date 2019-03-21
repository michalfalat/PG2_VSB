#include "optixtutorial.h"


struct TriangleAttributes
{
	optix::float3 normal;
	optix::float2 texcoord;
};


rtBuffer<uchar4, 2> output_buffer;
rtBuffer<optix::float3> normal_buffer;
rtBuffer<optix::float2, 1> texcoord_buffer;rtBuffer<optix::uchar1> material_index_buffer;
rtDeclareVariable( rtObject, top_object, , );
rtDeclareVariable( uint2, launch_dim, rtLaunchDim, );
rtDeclareVariable( uint2, launch_index, rtLaunchIndex, );
rtDeclareVariable( PerRayData_radiance, ray_data, rtPayload, );
rtDeclareVariable( float2, barycentrics, attribute rtTriangleBarycentrics, );
rtDeclareVariable(TriangleAttributes, attribs, attribute attributes, "Triangle attributes");rtDeclareVariable(optix::float3, view_from, , );
rtDeclareVariable(optix::Matrix3x3, M_c_w, , "camera to world space transformation matrix" );
rtDeclareVariable(float, focal_length, , "focal length in pixels" );

RT_PROGRAM void primary_ray( void )
{
	if ( launch_index.x == 0 && launch_index.y % 100 == 0 )
	{
		//rtPrintf("(%u, %u)\n", launch_index.x, launch_index.y);
	}

	const optix::float3 d_c = make_float3(launch_index.x -
		launch_dim.x * 0.5f, output_buffer.size().y * 0.5f -
		launch_index.y, -focal_length);
	const optix::float3 d_w = optix::normalize(M_c_w * d_c);
	optix::Ray ray(view_from, d_w, 0, 0.01f);

	PerRayData_radiance prd;
	rtTrace(top_object, ray, prd);
	output_buffer[launch_index] = optix::make_uchar4(prd.result.x*255.0f, prd.result.y*255.0f, prd.result.z*255.0f, 255);

	/*const optix::float3 d_c = make_float3(launch_index.x - output_buffer.size().x * 0.5f, output_buffer.size().y * 0.5f - launch_index.y, -focal_length);
	const optix::float3 d_w = optix::normalize(M_c_w * d_c);
	optix::Ray ray(view_from, d_w, 0, 0.01f);
	//optix::Ray ray( optix::make_float3( launch_index.x, launch_index.y, 1.0f ),
		//optix::normalize( optix::make_float3( 0.0f, 0.0f, -1.0f ) ), 0, 0.01f );
	PerRayData_radiance prd;
	rtTrace( top_object, ray, prd );

	// access to buffers within OptiX programs uses a simple array syntax	
	output_buffer[launch_index] = optix::make_uchar4( prd.result.x*255.0f, prd.result.y*255.0f, prd.result.z*255.0f, 255.0f );*/
}

/*RT_PROGRAM void attribute_program(void)
{
	const optix::float2 barycentrics = rtGetTriangleBarycentrics();
	const unsigned int index = rtGetPrimitiveIndex();
	const optix::float3 n0 = normal_buffer[index * 3 + 0];
	const optix::float3 n1 = normal_buffer[index * 3 + 0];
	const optix::float3 n2 = normal_buffer[index * 3 + 0];
		attribs.normal = optix::normalize(n1 * barycentrics.x + n2 * barycentrics.y +
			n0 * (1.0f - barycentrics.x - barycentrics.y));
}*/

RT_PROGRAM void closest_hit( void )
{
	const unsigned int index = rtGetPrimitiveIndex();
	const optix::float3 n0 = normal_buffer[index * 3 + 0];
	const optix::float3 n1 = normal_buffer[index * 3 + 1];
	const optix::float3 n2 = normal_buffer[index * 3 + 2];
	optix::float3 normal = optix::normalize(n1 * barycentrics.x + n2 * barycentrics.y + n0 * (1.0f - barycentrics.x - barycentrics.y));

	ray_data.result = optix::make_float3((normal.x + 1) / 2, (normal.y + 1) / 2, (normal.z + 1) / 2);
}

/* may access variables declared with the rtPayload semantic in the same way as closest-hit and any-hit programs */
RT_PROGRAM void miss_program( void )
{
	ray_data.result = optix::make_float3( 0.0f, 0.0f, 1.0f );
}

RT_PROGRAM void exception( void )
{
	const unsigned int code = rtGetExceptionCode();
	rtPrintf( "Exception 0x%X at (%d, %d)\n", code, launch_index.x, launch_index.y );
	rtPrintExceptionDetails();
	output_buffer[launch_index] = uchar4{ 255, 0, 255, 0 };
}
