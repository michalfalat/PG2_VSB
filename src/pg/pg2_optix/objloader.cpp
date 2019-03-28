﻿/*! \file objloader.cpp
\brief Načítání Wavefront OBJ souborů.
http://en.wikipedia.org/wiki/Wavefront_.obj_file
*/

#include "pch.h"
#include "material.h"
#include "utils.h"
#include "surface.h"
#include "mymath.h"

bool MaterialExists( std::vector<Material *> & materials, char * material_name )
{
	for ( Material * material : materials )
	{		
		if ( material->name().compare( material_name ) == 0 )
		{
			return true;
		}
	}

	return false;
}

Texture * TextureProxy(const std::string & full_name, std::map<std::string, Texture*> & already_loaded_textures,
	const int flip = -1, const bool single_channel = false )
{
	std::map<std::string, Texture*>::iterator already_loaded_texture = already_loaded_textures.find(full_name);
	Texture * texture = NULL;
	if (already_loaded_texture != already_loaded_textures.end())
	{
		texture = already_loaded_texture->second;
	}
	else
	{
		texture = new Texture( full_name.c_str() );// , flip, single_channel);
		already_loaded_textures[full_name] = texture;
	}

	return texture;
}

/*! \fn LoadMTL( const char * file_name, const char * path, std::vector<Material *> & materials )
\brief Načte materiály z MTL souboru \a file_name.
Soubor \a file_name se musí nacházet v cestě \a path. Načtené materiály budou vráceny přes pole \a materials.
\param file_name název MTL souboru včetně přípony.
\param path cesta k zadanému souboru.
\param materials pole materiálů, do kterého se budou ukládat načtené materiály.
*/
int LoadMTL( const char * file_name, const char * path, std::vector<Material *> & materials )
{
	// otevření soouboru
	FILE * file = fopen( file_name, "rt" );
	if ( file == NULL )
	{
		printf( "File %s not found.\n", file_name );

		return -1;
	}

	// načtení celého souboru do paměti	
	size_t file_size = static_cast<size_t>( GetFileSize64( file_name ) );	
	char * buffer = new char[file_size + 1]; // +1 protože budeme za poslední načtený byte dávat NULL
	char * buffer_backup = new char[file_size + 1];

	printf( "Loading materials from '%s' (%0.1f KB)...\n", file_name, file_size / 1024.0f );

	size_t number_of_items_read = fread( buffer, sizeof( *buffer ), file_size, file );

	// otestujeme korektnost načtení dat
	if ( !feof( file ) && ( number_of_items_read != file_size ) )
	{
		printf( "Unexpected end of file encountered.\n" );

		fclose( file );
		file = NULL;

		return -1;
	}	

	buffer[number_of_items_read] = 0; // zajistíme korektní ukončení řetězce

	fclose( file ); // ukončíme práci se souborem
	file = NULL;

	memcpy( buffer_backup, buffer, file_size + 1 ); // záloha bufferu

	printf( "Done.\n\n");

	printf( "Parsing mesh data...\n" );

	char material_name[128] = { 0 };
	char image_file_name[256] = { 0 };

	const char delim[] = "\n";
	char * line = strtok( buffer, delim );

	std::map<std::string, Texture*> already_loaded_textures;

	Material * material = NULL;
	int nextMaterialIndex = 0;

	// --- načítání všech materiálů ---
	while ( line != NULL )
	{
		if ( line[0] != '#' )
		{
			if ( strstr( line, "newmtl" ) == line )
			{
				if ( material != NULL )
				{
					material->set_name( material_name );
					if ( !MaterialExists( materials, material_name ) )
					{
						materials.push_back( material );
						printf( "\r%I64u material(s)\t\t", materials.size() );
					}
				}
				material = NULL;

				sscanf( line, "%*s %s", &material_name );
				//printf( "material name=%s\n", material_name );				

				material = new Material();
				material->matIndex = nextMaterialIndex;
				nextMaterialIndex++;
			}
			else
			{
				char * tmp = Trim( line );				
				if ( strstr( tmp, "Ka" ) == tmp ) // ambient color of the material
				{
					sscanf( tmp, "%*s %f %f %f", &material->ambient_.r, &material->ambient_.g, &material->ambient_.b );
					material->ambient_ = material->ambient_.linear();
				}
				else if ( strstr( tmp, "Kd" ) == tmp ) // diffuse color of the material
				{
					sscanf( tmp, "%*s %f %f %f", &material->diffuse_.r, &material->diffuse_.g, &material->diffuse_.b );
					material->diffuse_ = material->diffuse_.linear();
				}
				else if ( strstr( tmp, "Ks" ) == tmp ) // specular color of the material
				{
					sscanf( tmp, "%*s %f %f %f", &material->specular_.r, &material->specular_.g, &material->specular_.b );
					material->specular_ = material->specular_.linear();
				}
				else if ( strstr( tmp, "Ke" ) == tmp ) // emission color of the material
				{
					sscanf( tmp, "%*s %f %f %f", &material->emission_.r, &material->emission_.g, &material->emission_.b );
					//material->emission_ = material->emission_.linear();
				}
				else if ( strstr( tmp, "Ns" ) == tmp ) // specular coefficient
				{
					sscanf( tmp, "%*s %f", &material->shininess );
				}
				else if ( strstr( tmp, "map_Kd" ) == tmp ) // diffuse map
				{					
					sscanf( tmp, "%*s %s", image_file_name );
					std::string full_name = std::string( path ).append( image_file_name );
					material->set_texture( Material::kDiffuseMapSlot, TextureProxy( full_name, already_loaded_textures ) );
				}
				else if ( strstr( tmp, "map_Ks" ) == tmp ) // specular map
				{					
					sscanf( tmp, "%*s %s", image_file_name );
					std::string full_name = std::string( path ).append( image_file_name );
					material->set_texture( Material::kSpecularMapSlot, TextureProxy( full_name, already_loaded_textures ) );
				}
				else if ( strstr( tmp, "map_bump" ) == tmp ) // normal map
				{					
					sscanf( tmp, "%*s %s", image_file_name );
					std::string full_name = std::string(path).append(image_file_name);
					material->set_texture( Material::kNormalMapSlot, TextureProxy( full_name, already_loaded_textures ) );
				}
				else if ( strstr( tmp, "map_D" ) == tmp ) // opacity map
				{					
					sscanf( tmp, "%*s %s", image_file_name );
					std::string full_name = std::string(path).append(image_file_name);
					material->set_texture( Material::kOpacityMapSlot, TextureProxy( full_name, already_loaded_textures, -1, true ) );
				}
				else if ( strstr( tmp, "map_Pr" ) == tmp ) // roughness map
				{
					sscanf( tmp, "%*s %s", image_file_name );
					std::string full_name = std::string( path ).append( image_file_name );
					material->set_texture( Material::kRoughnessMapSlot, TextureProxy( full_name, already_loaded_textures, -1, true ) );
				}
				else if ( strstr( tmp, "map_Pm" ) == tmp ) // metallicness map
				{
					sscanf( tmp, "%*s %s", image_file_name );
					std::string full_name = std::string( path ).append( image_file_name );
					material->set_texture( Material::kMetallicnessMapSlot, TextureProxy( full_name, already_loaded_textures, -1, true ) );
				}
				else if ( strstr( tmp, "shader" ) == tmp ) // used shader
				{
					int shader = 0;
					sscanf( tmp, "%*s %d", &shader );
					material->set_shader( Shader( shader ) );
				}
				else if ( strstr( tmp, "Ni" ) == tmp || strstr( tmp, "ior" ) == tmp ) // index of refraction
				{					
					sscanf( tmp, "%*s %f", &material->ior );
				}
				else if ( strstr( tmp, "Pr" ) == tmp ) // roughness
				{
					sscanf( tmp, "%*s %f", &material->roughness_ );
				}
				else if ( strstr( tmp, "Pm" ) == tmp ) // metallicness
				{
					sscanf( tmp, "%*s %f", &material->metallicness );
				}
			}
		}

		line = strtok( NULL, delim ); // načtení dalšího řádku
	}

	if ( material != NULL )
	{
		material->set_name( material_name );
		materials.push_back( material );
		printf( "\r%I64u material(s)\t\t", materials.size() );
	}
	material = NULL;

	//memcpy( buffer, buffer_backup, file_size + 1 ); // obnovení bufferu po činnosti strtok
	SAFE_DELETE_ARRAY( buffer_backup );
	SAFE_DELETE_ARRAY( buffer );	

	printf( "\n" );

	return 0;
}

int LoadOBJ( const char * file_name, std::vector<Surface *> & surfaces, std::vector<Material *> & materials,
	const bool flip_yz , const Vector3 default_color )
{
	// otevření soouboru
	FILE * file = fopen( file_name, "rt" );
	if ( file == NULL )
	{
		printf( "File %s not found.\n", file_name );

		return -1;
	}

	// cesta k zadanému souboru
	char path[128] = { "" };
	const char * tmp = strrchr( file_name, '/' );
	if ( tmp != NULL )
	{
		memcpy( path, file_name, sizeof( char ) * ( tmp - file_name + 1 ) );
	}

	// načtení celého souboru do paměti
	/*const long long*/size_t file_size = static_cast<size_t>( GetFileSize64( file_name ) );
	char * buffer = new char[file_size + 1]; // +1 protože budeme za poslední načtený byte dávat NULL
	char * buffer_backup = new char[file_size + 1];	

	printf( "Loading model from '%s' (%0.1f MB)...\n", file_name, file_size / sqr( 1024.0f ) );

	size_t number_of_items_read = fread( buffer, sizeof( *buffer ), file_size, file );

	// otestujeme korektnost načtení dat
	if ( !feof( file ) && ( number_of_items_read != file_size ) )
	{
		printf( "Unexpected end of file encountered.\n" );

		fclose( file );
		file = NULL;

		return -1;
	}	

	buffer[number_of_items_read] = 0; // zajistíme korektní ukončení řetězce

	fclose( file ); // ukončíme práci se souborem
	file = NULL;

	memcpy( buffer_backup, buffer, file_size + 1 ); // záloha bufferu

	printf( "Done.\n\n");

	printf( "Parsing material data...\n" );

	char material_library[128] = { 0 };

	std::vector<std::string> material_libraries;	

	const char delim[] = "\n";
	char * line = strtok( buffer, delim );	

	// --- načítání všech materiálových knihoven, první průchod ---
	while ( line != NULL )
	{
		switch ( line[0] )
		{	
		case 'm': // mtllib
			{
				sscanf( line, "%*s %s", &material_library );
				printf( "Material library: %s\n", material_library );				
				material_libraries.push_back( std::string( path ).append( std::string( material_library ) ) );
			}
			break;
		}

		line = strtok( NULL, delim ); // načtení dalšího řádku
	}

	memcpy( buffer, buffer_backup, file_size + 1 ); // obnovení bufferu po činnosti strtok

	for ( int i = 0; i < static_cast<int>( material_libraries.size() ); ++i )
	{		
		LoadMTL( material_libraries[i].c_str(), path, materials );
	}

	std::vector<Vector3> vertices; // celý jeden soubor
	std::vector<Vector3> per_vertex_normals;
	std::vector<Coord2f> texture_coords;	

	line = strtok( buffer, delim );	
	//line = Trim( line );

	// --- načítání všech souřadnic, druhý průchod ---
	while ( line != NULL )
	{
		switch ( line[0] )
		{
		case 'v': // seznam vrcholů, normál nebo texturovacích souřadnic aktuální skupiny			
			{
				switch ( line[1] )
				{
				case ' ': // vertex
					{
						Vector3 vertex;
						if ( flip_yz )
						{
							//float x, y, z;
							sscanf( line, "%*s %f %f %f", &vertex.x, &vertex.z, &vertex.y );
							vertex.y *= -1;
						}
						else
						{
							sscanf( line, "%*s %f %f %f", &vertex.x, &vertex.y, &vertex.z );
						}

						vertices.push_back( vertex );
					}
					break;

				case 'n': // normála vertexu
					{
						Vector3 normal;
						if ( flip_yz )
						{			
							//float x, y, z;
							sscanf( line, "%*s %f %f %f", &normal.x, &normal.z, &normal.y );							
							normal.y *= -1;
						}
						else
						{
							sscanf( line, "%*s %f %f %f", &normal.x, &normal.y, &normal.z );
						}
						normal.Normalize();
						per_vertex_normals.push_back( normal );
					}
					break;

				case 't': // texturovací souřadnice
					{
						Coord2f texture_coord;
						float z = 0;
						sscanf( line, "%*s %f %f %f",
							&texture_coord.u, &texture_coord.v, &z );					
						texture_coords.push_back( texture_coord );
					}
					break;
				}
			}
			break;		
		}

		line = strtok( NULL, delim ); // načtení dalšího řádku
		//line = Trim( line );
	}

	memcpy( buffer, buffer_backup, file_size + 1 ); // obnovení bufferu po činnosti strtok

	printf( "%I64u vertices, %I64u normals and %I64u texture coords.\n",
		vertices.size(), per_vertex_normals.size(), texture_coords.size() );

	/// buffery pro načítání řetězců	
	char group_name[128];	
	char material_name[128];
	char vertices_indices[4][8 * 3 + 2];	// pomocný řetězec pro načítání indexů až 4 x "v/vt/vn"
	char vertex_indices[3][8];				// pomocný řetězec jednotlivých indexů "v", "vt" a "vn"	

	std::vector<Vertex> face_vertices; // pole všech vertexů právě načítané face

	int no_surfaces = 0; // počet načtených ploch

	line = strtok( buffer, delim ); // reset
	//line = Trim( line );

	// --- načítání jednotlivých objektů (group), třetí průchod ---
	while ( line != NULL )
	{
		switch ( line[0] )
		{
		case 'g': // group
			{
				if ( face_vertices.size() > 0 )
				{
					surfaces.push_back( BuildSurface( std::string( group_name ), face_vertices ) );
					printf( "\r%I64u group(s)\t\t", surfaces.size() );
					++no_surfaces;
					face_vertices.clear();

					for ( int i = 0; i < static_cast<int>( materials.size() ); ++i )
					{
						if ( materials[i]->name().compare( material_name ) == 0 )
						{
							Surface * s = *--surfaces.end();
							s->set_material( materials[i] );
							break;
						}
					}
				}

				sscanf( line, "%*s %s", &group_name );
				//printf( "Group name: %s\n", group_name );				
			}
			break;

		case 'u': // usemtl			
			{
				sscanf( line, "%*s %s", &material_name );
				//printf( "Material name: %s\n", material_name );						
			}
			break;

		case 'f': // face
			{
				// ! předpokládáme pouze trojúhelníky !
				// ! předpokládáme využití všech tří položek v/vt/vn !				
				int no_slashes = 0;
				for ( int i = 0; i < int( strlen( line ) ); ++i )
				{
					if ( line[i] == '/' )
					{
						++no_slashes;
					}
				}
				switch ( no_slashes )
				{
				case 2*3: // triangles
					sscanf( line, "%*s %s %s %s",
						&vertices_indices[0], &vertices_indices[1], &vertices_indices[2] );
					break;

				case 2*4: // quadrilaterals				
					sscanf( line, "%*s %s %s %s %s",
						&vertices_indices[0], &vertices_indices[1], &vertices_indices[2], &vertices_indices[3] );
					break;
				}

				// TODO smoothing groups

				for ( int i = 0; i < 3; ++i )				
				{									
					if (strstr(vertices_indices[i], "//"))
					{
						sscanf(vertices_indices[i], "%[0-9]//%[0-9]",
							&vertex_indices[0], &vertex_indices[2]);
						vertex_indices[1][0] = 0;
					}
					else
					{
						sscanf(vertices_indices[i], "%[0-9]/%[0-9]/%[0-9]",
							&vertex_indices[0], &vertex_indices[1], &vertex_indices[2]);
					}

					const int vertex_index = atoi( vertex_indices[0] ) - 1;					
					const int texture_coord_index = atoi( vertex_indices[1] ) - 1;
					const int per_vertex_normal_index = atoi( vertex_indices[2] ) - 1;

					if (texture_coord_index >= 0)
					{
						face_vertices.push_back(Vertex(vertices[vertex_index],
							per_vertex_normals[per_vertex_normal_index],
							default_color, &texture_coords[texture_coord_index]));
					}
					else
					{
						face_vertices.push_back(Vertex(vertices[vertex_index],
							per_vertex_normals[per_vertex_normal_index],
							default_color));
					}
					
				}

				if ( no_slashes == 2*4 )
				{
					const int i[] = { 0, 2, 3 };
					for ( int j = 0; j < 3; ++j )
					{				
						sscanf( vertices_indices[i[j]], "%[0-9]/%[0-9]/%[0-9]",					
							&vertex_indices[0], &vertex_indices[1], &vertex_indices[2] );

						const int vertex_index = atoi( vertex_indices[0] ) - 1;
						const int texture_coord_index = atoi( vertex_indices[1] ) - 1;
						const int per_vertex_normal_index = atoi( vertex_indices[2] ) - 1;

						face_vertices.push_back( Vertex( vertices[vertex_index],
							per_vertex_normals[per_vertex_normal_index],
							default_color, &texture_coords[texture_coord_index] ) );
					}
				}
			}
			break;
		}

		line = strtok( NULL, delim ); // načtení dalšího řádku
		//line = Trim( line );
	}

	if ( face_vertices.size() > 0 )
	{
		surfaces.push_back( BuildSurface( std::string( group_name ), face_vertices ) );
		printf( "\r%I64u group(s)\t\t", surfaces.size() );
		++no_surfaces;
		face_vertices.clear();

		for ( int i = 0; i < static_cast<int>( materials.size() ); ++i )
		{
			if ( materials[i]->name().compare( material_name ) == 0 )
			{
				Surface * s = *--surfaces.end();
				s->set_material( materials[i] );
				break;
			}
		}
	}

	texture_coords.clear();
	per_vertex_normals.clear();
	vertices.clear();	

	SAFE_DELETE_ARRAY( buffer_backup );
	SAFE_DELETE_ARRAY( buffer );	

	printf( "\nDone.\n\n");

	return no_surfaces;
}
