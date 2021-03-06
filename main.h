//------------------------------------------------------------------------------
//  Copyright 2007-2014 by Jyh-Ming Lien and George Mason University
//  See the file "LICENSE" for more information
//------------------------------------------------------------------------------


#pragma once

#include "objReader.h"
#include "model.h"
#include <list>
#include <float.h>
#include <math.h>
using namespace std;

//-----------------------------------------------------------------------------
// INPUTS
list<string> input_filenames;

//this defines the size of the lattice
unsigned int lattice_nx = 10, lattice_ny = 10, lattice_nz = 10; 

//-----------------------------------------------------------------------------
// Intermediate data
list<model> models;
float R=0;       //radius
Point3d COM;     //center of mass
float BoundingBox[6];

vector<Point3d> FFD_lattice; //This stores all lattice nodes, FFD_lattice has size = (lattice_nx X lattice_ny X lattice_nz)
vector<double> FFD_parameterization; //This stores all parameterized coordinates of all vertices from the model
//FFD_parameterization has size = models.front().v_size x (nx*ny*nz)

//-----------------------------------------------------------------------------
//read M+ from file
bool readfromfile();
void computeCOM_R();
void parameterizeModel();

//-----------------------------------------------------------------------------
bool parseArg(int argc, char ** argv)
{
    for(int i=1;i<argc;i++){
        if(argv[i][0]=='-')
        {
			if (string(argv[i]) == "-nx")      lattice_nx = atoi(argv[++i]);
			else if (string(argv[i]) == "-ny") lattice_ny = atoi(argv[++i]);
			else if (string(argv[i]) == "-nz") lattice_nz = atoi(argv[++i]);
			else
				return false; //unknown
        }
        else{
            input_filenames.push_back(argv[i]);
        }
    }

    return true;
}

void printUsage(char * name)
{
    //int offset=20;
    cerr<<"Usage: "<<name<<" [options] -nx integer -ny integer -nz integer *.obj \n"
        <<"options:\n\n";
    cerr<<"\n-- Report bugs to: Jyh-Ming Lien jmlien@gmu.edu"<<endl;
}

//-----------------------------------------------------------------------------

bool readfromfiles()
{
    long vsize=0;
    long fsize=0;

    uint id=0;
    for(list<string>::iterator i=input_filenames.begin();i!=input_filenames.end();i++,id++){
        cout<<"- ["<<id<<"/"<<input_filenames.size()<<"] Start reading "<<*i<<endl;
        model m;
        if(!m.build(*i)) continue;
        cout<<"- ["<<id<<"/"<<input_filenames.size()<<"] Done reading "<<m.v_size<<" vertices and "<<m.t_size<<" facets"<<endl;
        vsize+=m.v_size;
        fsize+=m.t_size;
        models.push_back(m);
    }
    cout<<"- Total: "<<vsize<<" vertices, "<<fsize<<" triangles, and "<<input_filenames.size()<<" models"<<endl;
    computeCOM_R();
	parameterizeModel();

    return true;
}

void computeCOM_R()
{
    //compute a bbox
    double box[6]={FLT_MAX,-FLT_MAX,FLT_MAX,-FLT_MAX,FLT_MAX,-FLT_MAX};
    //-------------------------------------------------------------------------
    for(list<model>::iterator i=models.begin();i!=models.end();i++){
        for(unsigned int j=0;j<i->v_size;j++){
            Point3d& p=i->vertices[j].p;
            if(p[0]<box[0]) box[0]=p[0];    //x_min
            if(p[0]>box[1]) box[1]=p[0];    //x_max
            if(p[1]<box[2]) box[2]=p[1];    //y_min
            if(p[1]>box[3]) box[3]=p[1];    //y_max
            if(p[2]<box[4]) box[4]=p[2];    //z_min
            if(p[2]>box[5]) box[5]=p[2];    //z_max
        }//j
    }//i
	
	//scale bbox and store in BoundingBox
	const float scale = 1.2;
	BoundingBox[0] = (box[1] + box[0]) / 2 - (box[1] - box[0])*scale / 2;
	BoundingBox[1] = (box[1] + box[0]) / 2 + (box[1] - box[0])*scale / 2;
	BoundingBox[2] = (box[3] + box[2]) / 2 - (box[3] - box[2])*scale / 2;
	BoundingBox[3] = (box[3] + box[2]) / 2 + (box[3] - box[2])*scale / 2;
	BoundingBox[4] = (box[5] + box[4]) / 2 - (box[5] - box[4])*scale / 2;
	BoundingBox[5] = (box[5] + box[4]) / 2 + (box[5] - box[4])*scale / 2;

	//build FFD_lattice here
	FFD_lattice = vector<Point3d>(lattice_nx*lattice_ny*lattice_nz, Point3d());
	float dx = (BoundingBox[1] - BoundingBox[0]) / (lattice_nx - 1);
	float dy = (BoundingBox[3] - BoundingBox[2]) / (lattice_ny - 1);
	float dz = (BoundingBox[5] - BoundingBox[4]) / (lattice_nz - 1);

	int i = 0;
	for (int ix = 0; ix < lattice_nx; ix++)
	{
		float x = BoundingBox[0] + ix*dx;
		for (int iy = 0; iy < lattice_ny; iy++)
		{
			float y = BoundingBox[2] + iy*dy;
			for (int iz = 0; iz < lattice_nz; iz++)
			{
				float z = BoundingBox[4] + iz*dz;
				FFD_lattice[i++] = Point3d(x, y, z);
			}//end for z
		}//end for y
	}//end for x

    //-------------------------------------------------------------------------
    // compute center of mass and R...
    COM.set( (box[1]+box[0])/2,(box[3]+box[2])/2,(box[5]+box[4])/2);

    //-------------------------------------------------------------------------
	R=0;
    for(list<model>::iterator i=models.begin();i!=models.end();i++){
        for(unsigned int j=0;j<i->v_size;j++){
            Point3d& p=i->vertices[j].p;
            float d=(float)(p-COM).normsqr();
            if(d>R) R=d;
        }//j
    }//i

    R=sqrt(R);
}

//convert each vertex of the model into parameterized space
//and store the parameterization in FFD_parameterization
void parameterizeModel()
{
    float xMin, xMax, yMin, yMax, zMin, zMax;  //for the sake of readability
 //    xMin = FFD_lattice[0][0];
 //    xMax = FFD_lattice[lattice_nx-1][0];
 //    yMin = FFD_lattice[lattice_nx * lattice_ny - 1][1];
 //    yMax = FFD_lattice[0][1];
 //    zMin = FFD_lattice[lattice_nx * lattice_ny * lattice_nz - 1][2];
 //    zMax = FFD_lattice[0][2]; //are these correct?
	xMin = BoundingBox[0];  xMax = BoundingBox[1];
    yMin = BoundingBox[2];  yMin = BoundingBox[3];
    zMin = BoundingBox[4];  zMin = BoundingBox[5];
    
    model& m = models.front(); 

	for (unsigned int i = 0; i < m.v_size; i++)
	{
		vertex & v = m.vertices[i];

        float px, py, pz; //parametization of x,y,z of a point
        px = ( (v.p[0] - xMin) / (xMax - xMin) );
        py = ( (v.p[1] - yMin) / (yMax - yMin) );
        pz = ( (v.p[2] - zMin) / (zMax - zMin) );

		//TODO: convert v.p (the position of p) into the parameterized space using FFD_lattice
		vector<double> weights; //say you have 24 lattice nodes, than 24 weights for each model vertex
        for (unsigned int c = 0; c < lattice_nz; c++)   //increment z
        {
            for (unsigned int b = 0; b < lattice_ny; b++)   //increment y
            {   
                for(unsigned int a = 0; a < lattice_nx; a++)    //increment x
                {
                    float dx, dy, dz, weight;  //xdirection, ydirection, zdirection, weight
                    dx = pow(px, a) * pow((1 - px), (lattice_nx - 1 - a));
                    dy = pow(py, b) * pow((1 - py), (lattice_ny - 1 - b));
                    dz = pow(pz, c) * pow((1 - pz), (lattice_nz - 1 - c));
                    weight = dx * dy * dz;
                    weights.push_back(weight);
                    //std::cout << "weight: " << weight << std::endl;
                    std::cout << "dx: " << dx << " dy: " << dy << " dz: " << dz << std::endl;
                }
            }
        }

		FFD_parameterization.insert(FFD_parameterization.end(), weights.begin(), weights.end());
	}
	//end i

	//done
}

//-----------------------------------------------------------------------------
//
//
//
//  Open GL stuff below
//
//
//-----------------------------------------------------------------------------

#include <draw.h>



