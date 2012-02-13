/*
 *     This file is part of FastVoxel.
 *
 *     FastVoxel is free software: you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation, either version 3 of the License, or
 *     (at your option) any later version.
 *
 *     FastVoxel is distributed in the hope that it will be useful,
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *     GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *     along with FastVoxel.  If not, see <http://www.gnu.org/licenses/>.
 * FastVoxel is a voxelisation library of polygonal 3d model and do volumes identifications.
 * It is dedicated to finite element solvers
 * @author Nicolas Fortin , Judica�l Picaut judicael.picaut (home) ifsttar.fr
 * Official repository is https://github.com/nicolas-f/FastVoxel
 */

#include "rply_interface.hpp"
#include "rply.h"
#include "en_numeric.hpp"

namespace formatRPLY
{
	struct parsing_instance
	{
		parsing_instance(t_model& _currentModel):currentModel(_currentModel),lastFaceSplited(false) {}
		t_model& currentModel;
		bool lastFaceSplited; /*!< La derni�re face avait 4 sommets et a �t� �clat� en deux triangles */
	};


	static int vertex_cb(p_ply_argument argument) {
		long plane;
		void* ptr;
		ply_get_argument_user_data(argument, &ptr, &plane);
		parsing_instance* curInstance((parsing_instance*)ptr);
		t_model* model=&(curInstance->currentModel);
		switch (plane) {
			case 0:
				model->modelVertices.push_back(vec3(ply_get_argument_value(argument),0,0));
				break;
			case 1:
				model->modelVertices.back().y=ply_get_argument_value(argument);
				break;
			case 2:
				model->modelVertices.back().z=ply_get_argument_value(argument);
				break;
		}
		return 1;
	}

	static int face_cb(p_ply_argument argument) {
		long length,idvert;
		void* ptr;
		ply_get_argument_user_data(argument, &ptr, NULL);
		ply_get_argument_property(argument, NULL, &length, &idvert);
		parsing_instance* curInstance((parsing_instance*)ptr);
		t_model* model=&(curInstance->currentModel);
		switch (idvert) {
			case 0:
				curInstance->lastFaceSplited=false;
				model->modelFaces.push_back(t_face(ivec3((int)ply_get_argument_value(argument),0,0)));
				break;
			case 1:
			case 2:
				model->modelFaces.back().indicesSommets[idvert]=(int)ply_get_argument_value(argument);
				break;
			case 3:
				//Polygone � quatre sommets
				const ivec3& lastTri(model->modelFaces.back().indicesSommets);
				model->modelFaces.push_back(t_face(ivec3(lastTri[0],lastTri[2],(int)ply_get_argument_value(argument))));
				curInstance->lastFaceSplited=true;
				break;
		}
		return 1;
	}
	static int face_layer_cb(p_ply_argument argument) {
		void* ptr;
		ply_get_argument_user_data(argument, &ptr, NULL);
		parsing_instance* curInstance((parsing_instance*)ptr);
		t_model* model=&(curInstance->currentModel);
		model->modelFacesLayerIndex.push_back((int)ply_get_argument_value(argument));
		if(curInstance->lastFaceSplited)
			model->modelFacesLayerIndex.push_back((int)ply_get_argument_value(argument));
		return 1;
	}

	static int layer_cb(p_ply_argument argument) {
		long length,idchar;
		void* ptr;
		ply_get_argument_user_data(argument, &ptr, NULL);
		ply_get_argument_property(argument, NULL, &length, &idchar);
		parsing_instance* curInstance((parsing_instance*)ptr);
		t_model* model=&(curInstance->currentModel);
		if(idchar==-1)
		{
			std::size_t sizeOfString((int)ply_get_argument_value(argument));
			model->modelLayers.push_back(std::string(sizeOfString,' '));
		}else{
			model->modelLayers.back().layerName.replace(idchar,1,1,(unsigned char)ply_get_argument_value(argument));
		}
		return 1;
	}

    class CloseHandle
	{
	public:
		CloseHandle(p_ply _PlyFileToClose)
			:PlyFileToClose(_PlyFileToClose)
		{

		}
		~CloseHandle()
		{
			ply_close(PlyFileToClose);
		}
	private:
		p_ply PlyFileToClose;
	};


	bool CPly::ImportPly(t_model& sceneconst, std::string mfilename)
	{
		EnglishTemporaryLocale dotNumericOnly;

		sceneconst.modelFaces.clear();
		sceneconst.modelVertices.clear();
		p_ply plyFile=ply_open(mfilename.c_str(),NULL);
		if(!plyFile)
			return false;
		CloseHandle plyCloseObj(plyFile);
		if (!ply_read_header(plyFile))
			return false;

		//setup_callbacks(plyFile);
		parsing_instance curInstance(sceneconst);

		ply_set_read_cb(plyFile, "vertex", "x", &vertex_cb, &curInstance, 0);
	    ply_set_read_cb(plyFile, "vertex", "y", &vertex_cb, &curInstance, 1);
		ply_set_read_cb(plyFile, "vertex", "z", &vertex_cb, &curInstance, 2);

		ply_set_read_cb(plyFile, "face", "vertex_indices", &face_cb, &curInstance, 0);
		ply_set_read_cb(plyFile, "face", "layer_id", &face_layer_cb, &curInstance, 0);

		ply_set_read_cb(plyFile, "layer", "layer_name", &layer_cb, &curInstance, 0);

		if (!ply_read(plyFile)) return false;


		return true;
	}

	bool CPly::ExportPly(t_model& scene, std::string mfilename)
	{
		EnglishTemporaryLocale dotNumericOnly;
		p_ply oply;
		oply = ply_create(mfilename.c_str(), PLY_BIG_ENDIAN, NULL);
		if (!oply) return false;
		CloseHandle plyCloseObj(oply);
		std::size_t faceCount(scene.modelFaces.size());
		std::size_t faceLayerCount(scene.modelFaces.size());
		bool useLayers=(!scene.modelLayers.empty() && faceLayerCount==faceCount);

		ply_add_element(oply, "vertex", scene.modelVertices.size());
		ply_add_property(oply,"x",PLY_FLOAT,PLY_FLOAT,PLY_FLOAT);
		ply_add_property(oply,"y",PLY_FLOAT,PLY_FLOAT,PLY_FLOAT);
		ply_add_property(oply,"z",PLY_FLOAT,PLY_FLOAT,PLY_FLOAT);
		ply_add_element(oply, "face", faceCount );
		ply_add_list_property(oply,"vertex_indices", PLY_UCHAR, PLY_INT );
		if(useLayers)
		{
			ply_add_property(oply,"layer_id",PLY_INT,PLY_INT,PLY_INT);
		}

		if(useLayers)
		{
			ply_add_element(oply, "layer", scene.modelLayers.size());
			ply_add_list_property(oply,"layer_name",PLY_UCHAR, PLY_UCHAR );
		}
		ply_write_header(oply);
		for(std::list<vec3>::iterator itvert=scene.modelVertices.begin();itvert!=scene.modelVertices.end();itvert++)
		{
			ply_write(oply,itvert->x);
			ply_write(oply,itvert->y);
			ply_write(oply,itvert->z);
		}
		std::list<std::size_t>::iterator itLayerIndex;
		if(useLayers)
		{
			itLayerIndex=scene.modelFacesLayerIndex.begin();
		}
		for(std::list<t_face>::iterator itface=scene.modelFaces.begin();itface!=scene.modelFaces.end();itface++)
		{
			ply_write(oply,3);
			ply_write(oply,itface->indicesSommets.a);
			ply_write(oply,itface->indicesSommets.b);
			ply_write(oply,itface->indicesSommets.c);
			if(useLayers)
			{
				ply_write(oply,*itLayerIndex);
				itLayerIndex++;
			}
		}
		for(std::list<t_layer>::iterator itLayer=scene.modelLayers.begin();itLayer!=scene.modelLayers.end();itLayer++)
		{
			const char* layerString=(*itLayer).layerName.c_str();
			std::size_t stringSize((*itLayer).layerName.size());
			ply_write(oply,(int)stringSize);
			for(std::size_t i=0;i<stringSize;i++)
			{
				ply_write(oply,(unsigned int)layerString[i]);
			}
		}
		return true;
	}

}
