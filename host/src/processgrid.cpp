/*

AutoDock-GPU, an OpenCL implementation of AutoDock 4.2 running a Lamarckian Genetic Algorithm
Copyright (C) 2017 TU Darmstadt, Embedded Systems and Applications Group, Germany. All rights reserved.
For some of the code, Copyright (C) 2019 Computational Structural Biology Center, the Scripps Research Institute.

AutoDock is a Trade Mark of the Scripps Research Institute.

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

*/


#include "processgrid.h"

int get_gridinfo(
                 const char*     fldfilename,
                       Gridinfo* mygrid
                )
{
	if(mygrid->info_read) return 0; // already succesfully read this grid's information

	std::ifstream fp;
	std::string line;
	char   tempstr [256];
	int    gpoints_even[3];
	int    recnamelen;
	double center[3];
	int grid_types=0;

	// ----------------------------------------------------
	// Getting full path fo the grid file
	// Getting father directory name
	//char* dir = dirname(ts1);
	//char* filename = basename(ts1);

	char* ts1 = strdup(fldfilename);
	#ifndef _WIN32
	mygrid->grid_file_path = strdup(dirname(ts1));
	#else
	char drive_tmp[_MAX_DRIVE];
	char path_tmp[_MAX_DIR];
	_splitpath(ts1, drive_tmp, path_tmp, NULL, NULL);
	
	char result[_MAX_DRIVE+_MAX_DIR];
	strcpy(result, drive_tmp);
	strcat(result, path_tmp);
	mygrid->grid_file_path = strdup(result);
	#endif
	free(ts1); // clean up
	// ----------------------------------------------------

	// Processing fld file
	fp.open(fldfilename);
	if (fp.fail())
	{
		printf("Error: can't open fld file %s!\n", fldfilename);
		return 1;
	}

	const char* ext = strstr(fldfilename,".maps");
	if(ext){
		int len=ext-fldfilename;
		mygrid->map_base_name = (char*)malloc((len+1)*sizeof(char));
		strncpy(mygrid->map_base_name,fldfilename,len);
		mygrid->map_base_name[len]='\0';
	} else{
		int len=strlen(fldfilename)+1;
		mygrid->map_base_name = (char*)malloc(len*sizeof(char));
		strcpy(mygrid->map_base_name,fldfilename);
	}

	while(std::getline(fp, line))
	{
		sscanf(line.c_str(),"%255s",tempstr);
		// -----------------------------------
		// Reorder according to file *.maps.fld
		// -----------------------------------
		// Grid spacing
		if (strcmp(tempstr, "#SPACING") == 0)
		{
			sscanf(&line.c_str()[8], "%lf", &(mygrid->spacing));
			if (mygrid->spacing > 1)
			{
				printf("Error: grid spacing is too big!\n");
				return 1;
			}
		}

		// capturing number of grid points
		if (strcmp(tempstr, "#NELEMENTS") == 0)
		{
			sscanf(&line.c_str()[10], "%d %d %d", &(gpoints_even[0]), &(gpoints_even[1]), &(gpoints_even[2]));
			// plus one gridpoint in each dimension
			mygrid->size_xyz[0] = gpoints_even[0] + 1;
			mygrid->size_xyz[1] = gpoints_even[1] + 1;
			mygrid->size_xyz[2] = gpoints_even[2] + 1;

			// If the grid is too big, send message and change the value of truncated_size_xyz
			if ((mygrid->size_xyz [0] > MAX_NUM_GRIDPOINTS) || (mygrid->size_xyz [1] > MAX_NUM_GRIDPOINTS) || (mygrid->size_xyz [2] > MAX_NUM_GRIDPOINTS))
			{
				printf("Error: each dimension of the grid must be below %i.\n", MAX_NUM_GRIDPOINTS);
				return 1;
			}
		}

		// Capturing center
		if (strcmp(tempstr, "#CENTER") == 0)
		{
			sscanf(&line.c_str()[7], "%lf %lf %lf", &(center[0]), &(center[1]), &(center[2]));
		}

		// Name of the receptor and corresponding files
		if (strcmp(tempstr, "#MACROMOLECULE") == 0)
		{
			sscanf(&line.c_str()[14], "%255s", tempstr);
			recnamelen = strcspn(tempstr,".");
			tempstr[recnamelen] = '\0';
			int len = strlen(tempstr)+1;
			mygrid->receptor_name = (char*)malloc(len*sizeof(char));
			strcpy(mygrid->receptor_name, tempstr);
		}

		if (line.find("label=") == 0)
		{
			sscanf(&line.c_str()[6],"%255s", tempstr);
			char* typesep = strchr(tempstr,'-'); // <atom type>-affinity ...
			if(typesep!=NULL){
				typesep[0]='\0'; // tempstr is now just the atom type
			} else{
				tempstr[1]='\0'; // tempstr is now either E(lectrostatics) or D(esolvation)
				tempstr[0]=tolower(tempstr[0]); // lower-case it
			}
			mygrid->grid_mapping.push_back(tempstr);
			grid_types++;
		}
		
		if (strcmp(tempstr, "variable") == 0)
		{
			size_t fidx = line.find("file=");
			if(fidx==std::string::npos){
				printf("Error: grid map file names cannot be read.\n");
				return 1;
			}
			sscanf(&line.c_str()[fidx+5],"%255s", tempstr);
			mygrid->grid_mapping.push_back(tempstr);
		}
	}
	
	if(mygrid->grid_mapping.size() != 2*grid_types){
		printf("Error: Number of grid map labels (%d) and filenames (%d) mismatched in fld file.\n", grid_types, mygrid->grid_mapping.size()-grid_types);
		return 1;
	}

	// calculating grid size
	mygrid->size_xyz_angstr[0] = (mygrid->size_xyz[0]-1)*(mygrid->spacing);
	mygrid->size_xyz_angstr[1] = (mygrid->size_xyz[1]-1)*(mygrid->spacing);
	mygrid->size_xyz_angstr[2] = (mygrid->size_xyz[2]-1)*(mygrid->spacing);

	// calculating coordinates of origo
	mygrid->origo_real_xyz[0] = center[0] - (((double) gpoints_even[0])*0.5*(mygrid->spacing));
	mygrid->origo_real_xyz[1] = center[1] - (((double) gpoints_even[1])*0.5*(mygrid->spacing));
	mygrid->origo_real_xyz[2] = center[2] - (((double) gpoints_even[2])*0.5*(mygrid->spacing));

	fp.close();
	mygrid->info_read = true;

	return 0;
}

int get_gridvalues_f(
                     const Gridinfo* mygrid,
                           float**   fgrids
                    )
{
	*fgrids = (float*) malloc(4*(sizeof(float))*(mygrid->num_of_atypes+2)*
	                                            (mygrid->size_xyz[0])*
	                                            (mygrid->size_xyz[1])*
	                                            (mygrid->size_xyz[2]));
	if (*fgrids == NULL)
	{
		printf("Error: not enough memory!\n");
		return 1;
	}
	return get_gridvalues_f(mygrid, *fgrids);
}

int get_gridvalues_f(
                     const Gridinfo* mygrid,
                           float*    fgrids
                    )
// The function reads the grid point values from the .map files
// that correspond to the receptor given by the first parameter.
// It allocates the proper amount of memory and stores the data there,
// which can be accessed with the fgrids pointer.
// If there are any errors, it returns 1, otherwise
// the return value is 0.
{
	int t, ti, x, y, z;
	std::ifstream fp;
	std::string line;
	size_t len = strlen(mygrid->grid_file_path)+strlen(mygrid->receptor_name)+1;
	if(strlen(mygrid->map_base_name)>len)
		len = strlen(mygrid->map_base_name);
	len += 10; // "..map\0" = 6 entries + 4 at most for grid type
	if(len<128) len=128;
	char* tempstr = (char*)malloc(len*sizeof(char));
	float* mypoi;

	unsigned int g1 = mygrid->size_xyz[0];
	unsigned int g2 = g1*mygrid->size_xyz[1];
	mypoi = fgrids;

	for (t=0; t < mygrid->num_of_atypes+2; t++)
	{
		// find corresponding fld entry
		ti=-1;
		for (x=0; x<(mygrid->grid_mapping.size()/2); x++){
			if(mygrid->grid_mapping[x].find(mygrid->ligand_grid_types[t]) == 0){
				ti=x+mygrid->grid_mapping.size()/2; // found
				break;
			}
		}
		if(ti<0){
			 // if no G-map is specified, none is used and the corresponding map is set to zeroes
			 // - parse_ligand() will exclude those atoms from contributing to interE
			if(strncmp(mygrid->ligand_grid_types[t],"G",1)==0){
				for (z=0; z < mygrid->size_xyz[2]; z++)
					for (y=0; y < mygrid->size_xyz[1]; y++)
						for (x=0; x < mygrid->size_xyz[0]; x++)
						{
							*mypoi = 0.0f;
							// fill in duplicate data for linearized memory access in kernel
							if(y>0) *(mypoi-4*g1+1) = *mypoi;
							if(z>0) *(mypoi-4*g2+2) = *mypoi;
							if(y>0 && z>0) *(mypoi-4*(g2+g1)+3) = *mypoi;
							mypoi+=4;
						}
				continue;
			} else{
				printf("Error: No map file specified for atom type in fld and no derived type (--derivtype, -T) either.\n");
				if (strncmp(mygrid->ligand_grid_types[t],"CG",2)==0)
					printf("       Expecting a derived type for each CGx (x=0..9) atom type (i.e. --derivtype CG0,CG1=C).\n");
				return 1;
			}
		}
		if(mygrid->fld_relative){
			strcpy(tempstr,mygrid->grid_file_path);
			strcat(tempstr, "/");
			strcat(tempstr, mygrid->grid_mapping[ti].c_str());
			fp.open(tempstr);
		}
		if (fp.fail())
		{
			printf("Error: Can't open grid map %s!\n", tempstr);
			return 1;
		}

		// seeking to first data
		do std::getline(fp, line);
		while (line.find("CENTER") != 0);

		// reading values
		for (z=0; z < mygrid->size_xyz[2]; z++)
			for (y=0; y < mygrid->size_xyz[1]; y++)
				for (x=0; x < mygrid->size_xyz[0]; x++)
				{
					std::getline(fp, line); sscanf(line.c_str(), "%f", mypoi);
					// fill in duplicate data for linearized memory access in kernel
					if(y>0) *(mypoi-4*g1+1) = *mypoi;
					if(z>0) *(mypoi-4*g2+2) = *mypoi;
					if(y>0 && z>0) *(mypoi-4*(g2+g1)+3) = *mypoi;
					mypoi+=4;
				}
		fp.close();
	}
	free(tempstr);
	return 0;
}

