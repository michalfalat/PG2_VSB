#include "pch.h"
#include "raytracer.h"
#include "objloader.h"
#include "tutorials.h"
#include "mymath.h"
#include "omp.h"
#include "utils.h"

void Raytracer::error_handler(RTresult code)
{
	if (code != RT_SUCCESS)
	{
		const char* error_string;
		rtContextGetErrorString(context_, code, &error_string);
		printf(error_string);
		throw std::runtime_error(error_string);
	}
}

Raytracer::Raytracer(const int width, const int height, const float fov_y, const Vector3 view_from, const Vector3 view_at) : SimpleGuiDX11(width, height)
{
	InitDeviceAndScene();
	camera = Camera(width, height, fov_y, view_from, view_at);
	fov = fov_y;
	speed = 3;
}

Raytracer::~Raytracer()
{
	ReleaseDeviceAndScene();
}

int Raytracer::InitDeviceAndScene()
{
	error_handler(rtContextCreate(&context_));
	error_handler(rtContextSetRayTypeCount(context_, 2));
	error_handler(rtContextSetEntryPointCount(context_, 1));
	error_handler(rtContextSetMaxTraceDepth(context_, 2));

	RTvariable output;
	error_handler(rtContextDeclareVariable(context_, "output_buffer", &output));
	error_handler(rtBufferCreate(context_, RT_BUFFER_OUTPUT, &output_buffer_));
	error_handler(rtBufferSetFormat(output_buffer_, RT_FORMAT_UNSIGNED_BYTE4));
	error_handler(rtBufferSetSize2D(output_buffer_, width(), height()));
	error_handler(rtVariableSetObject(output, output_buffer_));

	RTprogram primary_ray;
	error_handler(rtProgramCreateFromPTXFile(context_, "optixtutorial.ptx", "primary_ray", &primary_ray));
	error_handler(rtContextSetRayGenerationProgram(context_, 0, primary_ray));
	error_handler(rtProgramValidate(primary_ray));

	rtProgramDeclareVariable(primary_ray, "focal_length", &focal_length);
	rtProgramDeclareVariable(primary_ray, "view_from", &view_from);
	rtProgramDeclareVariable(primary_ray, "M_c_w", &M_c_w);

	rtVariableSet3f(view_from, camera.view_from().x, camera.view_from().y, camera.view_from().z);
	rtVariableSet1f(focal_length, camera.focalLength());
	rtVariableSetMatrix3x3fv(M_c_w, 0, camera.M_c_w().data());

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

	return S_OK;
}

int Raytracer::ReleaseDeviceAndScene()
{
	error_handler(rtContextDestroy(context_));
	return S_OK;
}

int Raytracer::InitGraph() {
	error_handler(rtContextValidate(context_));

	return S_OK;
}

int Raytracer::GetImage(BYTE * buffer) {
	camera.updateFov(fov);
	camera.recalculateMcw();
	rtVariableSet3f(view_from, camera.view_from().x, camera.view_from().y, camera.view_from().z);
	rtVariableSet1f(focal_length, camera.focalLength());
	rtVariableSetMatrix3x3fv(M_c_w, 0, camera.M_c_w().data());

	error_handler(rtContextLaunch2D(context_, 0, width(), height()));
	optix::uchar4 * data = nullptr;
	error_handler(rtBufferMap(output_buffer_, (void**)(&data)));
	memcpy(buffer, data, sizeof(optix::uchar4) * width() * height());
	error_handler(rtBufferUnmap(output_buffer_));
	return S_OK;
}

void Raytracer::LoadScene(const std::string file_name)
{
	const int no_surfaces = LoadOBJ(file_name.c_str(), surfaces_, materials_);

	int no_triangles = 0;

	for (auto surface : surfaces_)
	{
		no_triangles += surface->no_triangles();
	}

	RTgeometrytriangles geometry_triangles;
	error_handler(rtGeometryTrianglesCreate(context_, &geometry_triangles));
	error_handler(rtGeometryTrianglesSetPrimitiveCount(geometry_triangles, no_triangles));

	RTbuffer vertex_buffer;
	error_handler(rtBufferCreate(context_, RT_BUFFER_INPUT, &vertex_buffer));
	error_handler(rtBufferSetFormat(vertex_buffer, RT_FORMAT_FLOAT3));
	error_handler(rtBufferSetSize1D(vertex_buffer, no_triangles * 3));

	RTvariable normals;
	rtContextDeclareVariable(context_, "normal_buffer", &normals);
	RTbuffer normal_buffer;
	error_handler(rtBufferCreate(context_, RT_BUFFER_INPUT, &normal_buffer));
	error_handler(rtBufferSetFormat(normal_buffer, RT_FORMAT_FLOAT3));
	error_handler(rtBufferSetSize1D(normal_buffer, no_triangles * 3));

	RTvariable texcoords;
	rtContextDeclareVariable(context_, "texcoord_buffer", &texcoords);
	RTbuffer texcoord_buffer;
	error_handler(rtBufferCreate(context_, RT_BUFFER_INPUT, &texcoord_buffer));
	error_handler(rtBufferSetFormat(texcoord_buffer, RT_FORMAT_FLOAT2));
	error_handler(rtBufferSetSize1D(texcoord_buffer, no_triangles * 3));

	RTvariable materialIndices;
	rtContextDeclareVariable(context_, "material_buffer", &materialIndices);
	RTbuffer material_buffer;
	error_handler(rtBufferCreate(context_, RT_BUFFER_INPUT, &material_buffer));
	error_handler(rtBufferSetFormat(material_buffer, RT_FORMAT_UNSIGNED_BYTE));
	error_handler(rtBufferSetSize1D(material_buffer, no_triangles));

	optix::float3* vertexData = nullptr;
	optix::float3* normalData = nullptr;
	optix::uchar1* materialData = nullptr;
	optix::float2* texcoordData = nullptr;

	error_handler(rtBufferMap(vertex_buffer, (void**)(&vertexData)));
	error_handler(rtBufferMap(normal_buffer, (void**)(&normalData)));
	error_handler(rtBufferMap(material_buffer, (void**)(&materialData)));
	error_handler(rtBufferMap(texcoord_buffer, (void**)(&texcoordData)));

	// surfaces loop
	int k = 0, l = 0;
	for (auto surface : surfaces_)
	{
		// triangles loop
		for (int i = 0; i < surface->no_triangles(); ++i, ++l)
		{
			Triangle & triangle = surface->get_triangle(i);

			materialData[l].x = (unsigned char)surface->get_material()->matIndex;

			// vertices loop
			for (int j = 0; j < 3; ++j, ++k)
			{
				const Vertex & vertex = triangle.vertex(j);
				vertexData[k].x = vertex.position.x;
				vertexData[k].y = vertex.position.y;
				vertexData[k].z = vertex.position.z;
				//printf("%d \n", k);
				normalData[k].x = vertex.normal.x;
				normalData[k].y = vertex.normal.y;
				normalData[k].z = vertex.normal.z;

				texcoordData[k].x = vertex.texture_coords->u;
				texcoordData[k].y = vertex.texture_coords->v;
			} // end of vertices loop

		} // end of triangles loop

	} // end of surfaces loop

	rtBufferUnmap(normal_buffer);
	rtBufferUnmap(material_buffer);
	rtBufferUnmap(vertex_buffer);
	rtBufferUnmap(texcoord_buffer);

	rtBufferValidate(texcoord_buffer);
	rtVariableSetObject(texcoords, texcoord_buffer);

	rtBufferValidate(normal_buffer);
	rtVariableSetObject(normals, normal_buffer);

	rtBufferValidate(material_buffer);
	rtVariableSetObject(materialIndices, material_buffer);
	rtBufferValidate(vertex_buffer);

	error_handler(rtGeometryTrianglesSetMaterialCount(geometry_triangles, materials_.size()));
	error_handler(rtGeometryTrianglesSetMaterialIndices(geometry_triangles, material_buffer, 0, sizeof(optix::uchar1), RT_FORMAT_UNSIGNED_BYTE));
	error_handler(rtGeometryTrianglesSetVertices(geometry_triangles, no_triangles * 3, vertex_buffer, 0, sizeof(optix::float3), RT_FORMAT_FLOAT3));

	RTprogram attribute_program;
	error_handler(rtProgramCreateFromPTXFile(context_, "optixtutorial.ptx", "attribute_program", &attribute_program));
	error_handler(rtProgramValidate(attribute_program));
	error_handler(rtGeometryTrianglesSetAttributeProgram(geometry_triangles, attribute_program));

	error_handler(rtGeometryTrianglesValidate(geometry_triangles));

	// geometry instance
	RTgeometryinstance geometry_instance;
	error_handler(rtGeometryInstanceCreate(context_, &geometry_instance));
	error_handler(rtGeometryInstanceSetGeometryTriangles(geometry_instance, geometry_triangles));
	error_handler(rtGeometryInstanceSetMaterialCount(geometry_instance, materials_.size()));

	RTprogram any_hit;
	error_handler(rtProgramCreateFromPTXFile(context_, "optixtutorial.ptx", "any_hit", &any_hit));
	error_handler(rtProgramValidate(any_hit));

	int next_tex_diffuse_id = 0;
	for (Material* material : materials_) {
		RTmaterial rtMaterial;
		error_handler(rtMaterialCreate(context_, &rtMaterial));
		RTprogram closest_hit;
		error_handler(rtProgramCreateFromPTXFile(context_, "optixtutorial.ptx", "closest_hit_phong_shader", &closest_hit));

		

		error_handler(createAndSetMaterialColorVariable(rtMaterial, "diffuse", material->diffuse()));
		error_handler(createAndSetMaterialColorVariable(rtMaterial, "specular", material->specular()));
		error_handler(createAndSetMaterialColorVariable(rtMaterial, "ambient", material->ambient()));
		error_handler(createAndSetMaterialScalarVariable(rtMaterial, "shininess", material->shininess));

		RTvariable tex_diffuse_id;
		rtMaterialDeclareVariable(rtMaterial, "tex_diffuse_id", &tex_diffuse_id);

		if (material->texture(material->kDiffuseMapSlot) != NULL) {
			RTtexturesampler textureSampler;
			rtTextureSamplerCreate(context_, &textureSampler);
			int texture_id;
			rtTextureSamplerGetId(textureSampler, &texture_id);

			optix::float4* textureData = nullptr;

			rtVariableSet1i(tex_diffuse_id, texture_id);
			Texture* texture = material->texture(material->kDiffuseMapSlot);
			RTbuffer texture_buffer;
			error_handler(rtBufferCreate(context_, RT_BUFFER_INPUT, &texture_buffer));
			error_handler(rtBufferSetFormat(texture_buffer, RT_FORMAT_FLOAT4));
			error_handler(rtBufferSetSize2D(texture_buffer, texture->width(), texture->height()));
			error_handler(rtBufferMap(texture_buffer, (void**)(&textureData)));

			for (int i = 0; i < (texture->height() * texture->width()); i++) {
				textureData[i] = optix::make_float4(texture->getData()[3 * i] / 255.0f, texture->getData()[3 * i + 1] / 255.0f, texture->getData()[3 * i + 2] / 255.0f, 1);
			}

			rtTextureSamplerSetReadMode(textureSampler, RT_TEXTURE_READ_NORMALIZED_FLOAT);


			error_handler(rtBufferUnmap(texture_buffer));
			error_handler(rtTextureSamplerSetBuffer(textureSampler, 0, 0, texture_buffer));
			error_handler(rtTextureSamplerValidate(textureSampler));
		}
		else {
			rtVariableSet1i(tex_diffuse_id, -1);
		}

		error_handler(rtProgramValidate(closest_hit));
		error_handler(rtMaterialSetClosestHitProgram(rtMaterial, 0, closest_hit));
		error_handler(rtMaterialSetAnyHitProgram(rtMaterial, 1, any_hit));
		error_handler(rtMaterialValidate(rtMaterial));

		error_handler(rtGeometryInstanceSetMaterial(geometry_instance, material->matIndex, rtMaterial));
	}
	error_handler(rtGeometryInstanceValidate(geometry_instance));

	// acceleration structure
	RTacceleration sbvh;
	error_handler(rtAccelerationCreate(context_, &sbvh));
	error_handler(rtAccelerationSetBuilder(sbvh, "Sbvh"));
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
}

int Raytracer::Ui()
{
	static float f = 0.0f;
	static int counter = 0;

	// we use a Begin/End pair to created a named window
	ImGui::Begin("Ray Tracer Params");

	ImGui::SliderInt("'Speed", &speed, 0, 10);
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

	bool arrowUpPressed = GetKeyState(VK_UP) & 0x8000 ? true : false;
	bool arrowDownPressed = GetKeyState(VK_DOWN) & 0x8000 ? true : false;
	bool arrowLeftPressed = GetKeyState(VK_LEFT) & 0x8000 ? true : false;
	bool arrowRightPressed = GetKeyState(VK_RIGHT) & 0x8000 ? true : false;
	bool wPressed = GetKeyState('W') & 0x8000 ? true : false;
	bool aPressed = GetKeyState('A') & 0x8000 ? true : false;
	bool sPressed = GetKeyState('S') & 0x8000 ? true : false;
	bool dPressed = GetKeyState('D') & 0x8000 ? true : false;
	bool zPressed = GetKeyState('Z') & 0x8000 ? true : false;
	bool cPressed = GetKeyState('C') & 0x8000 ? true : false;

	float time = ImGui::GetIO().DeltaTime * 60;

	double frameStep = speed * time;

	if (arrowUpPressed) camera.moveForward(frameStep);
	if (arrowDownPressed) camera.moveForward(-frameStep);
	if (arrowRightPressed) camera.moveRight(frameStep);
	if (arrowLeftPressed) camera.moveRight(-frameStep);
	if (dPressed) camera.rotateRight(frameStep);
	if (aPressed) camera.rotateRight(-frameStep);
	if (sPressed) camera.rotateUp(-frameStep);
	if (wPressed) camera.rotateUp(frameStep);
	if (cPressed) camera.rollRight(frameStep);
	if (zPressed) camera.rollRight(-frameStep);
	ImGui::End();


	return 0;
}