
/**
 * @file nstroke_tracing.cpp
 * @brief n-right-strokes curve drawing (refine)
 * This curve drawing method performs as follows:
 *  1. The user first draws the primary curve using right-mouse moving. This drawing
 *     is based on the method used in solveCurveLineInter() and line intersection.
 *     The intersection between the line of last two locs and the viewing line.
 *
1) smCurveLineInter:
(1) get the nearest line (pa,pb) between line (t-2,t-1) and (loc0,loc1), where
 t-2, t-1 are last two loc of the traced curve, loc0,loc1 are (near,far) loc on
 the viewing line;
(2) get the mass center between (pa,pb) as the candidate loc by using function
  getCenterOfLineProfile().
2) smCurveDirectionInter:
(1) get the nearest line (pa,pb) between line (t-2,t-1) and (loc0,loc1). Set pb
  as one of candidate loc values;
(2) get the mass center between (loc0,loc1) as another candidate loc by using function
  getCenterOfLineProfile();
(3) compare signal (mean+std) difference of two candidate locs and last two traced locs.
 the candidate loc with less difference is the loc we want.

 * @author: Jianlong Zhou
 * @date: Jan 24, 2012
 *
 */

#include "renderer_gl1.h"
#include "v3dr_glwidget.h"
#include "barFigureDialog.h"
#include "v3d_application.h"

#ifndef test_main_cpp

#include "../v3d/v3d_compile_constraints.h"
#include "../v3d/landmark_property_dialog.h"
#include "../v3d/surfaceobj_annotation_dialog.h"
#include "../v3d/surfaceobj_geometry_dialog.h"
#include "../neuron_editing/neuron_sim_scores.h"
#include "../neuron_tracing/neuron_tracing.h"
#include "../basic_c_fun/v3d_curvetracepara.h"
#include "../v3d/dialog_curve_trace_para.h"
#include <map>
#include <set>
#include <time.h>
#endif //test_main_cpp

#include "../neuron_tracing/fastmarching_linker.h"

#define EPS 0.01

#define PI 3.14159265

#ifndef MIN
#define MIN(a, b)  ( ((a)<(b))? (a) : (b) )
#endif
#ifndef MAX
#define MAX(a, b)  ( ((a)>(b))? (a) : (b) )
#endif

#define INSERT_NEIGHBOR(nei) {if (dataViewProcBox.isInner(nei, 0.5)) neibs_loci.push_back(nei);}



void Renderer_gl1::solveCurveDirectionInter(vector <XYZ> & loc_vec_input, vector <XYZ> &loc_vec, int index)
{
	bool b_use_seriespointclick = (loc_vec_input.size()>0) ? true : false;
     if (b_use_seriespointclick==false && list_listCurvePos.size()<1)  return;

	bool b_use_last_approximate=true;

#ifndef test_main_cpp
	MainWindow* V3Dmainwindow = 0;
	V3Dmainwindow = v3dr_getV3Dmainwindow(_idep);
	if (V3Dmainwindow)
		b_use_last_approximate = V3Dmainwindow->global_setting.b_3dcurve_inertia;
#endif

	////////////////////////////////////////////////////////////////////////
	int chno = checkCurChannel();
	if (chno<0 || chno>dim4-1)   chno = 0; //default first channel
	////////////////////////////////////////////////////////////////////////
	qDebug()<<"\n solveCurveDirectionInter: 3d curve in channel # "<<((chno<0)? chno :chno+1);

	loc_vec.clear();

     // add intersection point into listMarker
     listMarker.clear();
     int totalm=0;//total intersection point

	int N = loc_vec_input.size();
	if (b_use_seriespointclick)
	{
		loc_vec = loc_vec_input;
	}
	else //then use the moving mouse location, otherwise using the preset loc_vec_input (which is set by the 3d-curve-by-point-click function)
	{
		N = list_listCurvePos.at(index).size(); // change from 0 to index for different curves, ZJL
		for (int i=0; i<N; i++)
		{
			const MarkerPos & pos = list_listCurvePos.at(index).at(i); // change from 0 to index for different curves, ZJL
			////////////////////////////////////////////////////////////////////////
			//100730 RZC, in View space, keep for dot(clip, pos)>=0
			double clipplane[4] = { 0.0,  0.0, -1.0,  0 };
			clipplane[3] = viewClip;
			ViewPlaneToModel(pos.MV, clipplane);
			//qDebug()<<"   clipplane:"<<clipplane[0]<<clipplane[1]<<clipplane[2]<<clipplane[3];
			////////////////////////////////////////////////////////////////////////

			XYZ loc0, loc1;
			_MarkerPos_to_NearFarPoint(pos, loc0, loc1);

			XYZ loc;
			float length01 = dist_L2(loc0, loc1);
			if (length01<1.0)
			{
				loc=(loc0+loc1)/2.0;
			}
			else
			{
                    int NL = loc_vec.size();
				int last_j = NL-1;
				int last_j2 = NL-2;
				XYZ lastpos, lastpos2;
                    if (last_j>=0 && b_use_last_approximate) //091114 PHC make it more stable by conditioned on previous location
				{
					lastpos = loc_vec.at(last_j);
                         if (dataViewProcBox.isInner(lastpos, 0.5))
					{
						XYZ v_1_0 = loc1-loc0, v_0_last=loc0-lastpos;
						XYZ nearestloc = loc0-v_1_0*dot(v_0_last, v_1_0)/dot(v_1_0, v_1_0); //since loc0!=loc1, this is safe

						double ranget = (length01/2.0)>10?10:(length01/2.0); //at most 30 pixels aparts

						XYZ D = v_1_0; normalize(D);
						loc0 = nearestloc - D*(ranget);
						loc1 = nearestloc + D*(ranget);
                         }
                    }

				// determine loc based on intersection of vector (lastpos2, lastpos) and (loc0,loc1)
				if (last_j2>=0)
				{
					XYZ pa, pb;
					double mua, mub;
					lastpos2 = loc_vec.at(last_j2);
					bool res=lineLineIntersect(lastpos2, lastpos, loc0, loc1, &pa, &pb, &mua, &mub);
                         if(!res)
                         {
                              //no closest pos
                              qDebug()<<"No intersection checking resutls!";
                              loc=getCenterOfLineProfile(loc0, loc1, clipplane, chno);
                         }else
                         {
                              // to see whether this is the right loc by comparing mean+sdev
                              if(selectMode==smCurveDirectionInter)
                              {
                                   XYZ loci = pb;

                                   // check if pb is inside line seg of(loc0,loc1)
                                   // confine pb inside (loc0,loc1), if outside, then use locc directly
                                   if( !withinLineSegCheck( loc0, loc1, loci ) )
                                   {
                                        // pb is outside (loc0,loc), so search in (loc0,loc1)
                                        loc=getCenterOfLineProfile(loc0, loc1, clipplane, chno);
                                        //addMarker(loc); // for comparison purpose, delete it later
                                   }else
                                   {
                                        // search loc within a smaller ranger around loci
                                        XYZ v_1_0 = loc1-loc0;
                                        float length = dist_L2(loc0, loc1);
                                        float ranget = length/5.0;
                                        XYZ D = v_1_0; normalize(D);
                                        loc0 = loci - D*(ranget);
                                        loc1 = loci + D*(ranget);
                                        loc = getCenterOfLineProfile(loc0, loc1, clipplane, chno);
                                   }

                              }
                         }


				} else // for first two locs
				{
					loc = getCenterOfLineProfile(loc0, loc1, clipplane, chno);
				}
			}


			if (dataViewProcBox.isInner(loc, 0.5))
			{
				dataViewProcBox.clamp(loc); //100722 RZC
				loc_vec.push_back(loc);
			}
		}
	}

     if(loc_vec.size()<1) return; // all points are outside the volume. ZJL 110913

#ifndef test_main_cpp
	// check if there is any existing neuron node is very close to the starting and ending points, if yes, then merge

	N = loc_vec.size(); //100722 RZC
	if (V3Dmainwindow && V3Dmainwindow->global_setting.b_3dcurve_autoconnecttips && b_use_seriespointclick==false)
	{
		if (listNeuronTree.size()>0 && curEditingNeuron>0 && curEditingNeuron<=listNeuronTree.size())
		{
			NeuronTree *p_tree = (NeuronTree *)(&(listNeuronTree.at(curEditingNeuron-1)));
			if (p_tree)
			{
                    // at(0) to at(index) ZJL 110901
				V3DLONG n_id_start = findNearestNeuronNode_WinXY(list_listCurvePos.at(index).at(0).x, list_listCurvePos.at(index).at(0).y, p_tree);
				V3DLONG n_id_end = findNearestNeuronNode_WinXY(list_listCurvePos.at(index).at(N-1).x, list_listCurvePos.at(index).at(N-1).y, p_tree);
                                qDebug("detect nearest neuron node [%ld] for curve-start and node [%ld] for curve-end for the [%d] neuron", n_id_start, n_id_end, curEditingNeuron);

				double th_merge = 5;

				bool b_start_merged=false, b_end_merged=false;
				NeuronSWC cur_node;
				if (n_id_start>=0)
				{
					cur_node = p_tree->listNeuron.at(n_id_start);
					qDebug()<<cur_node.x<<" "<<cur_node.y<<" "<<cur_node.z;
					XYZ cur_node_xyz = XYZ(cur_node.x, cur_node.y, cur_node.z);
					if (dist_L2(cur_node_xyz, loc_vec.at(0))<th_merge)
					{
						loc_vec.at(0) = cur_node_xyz;
						b_start_merged = true;
						qDebug()<<"force set the first point of this curve to the above neuron node as they are close.";
					}
				}
				if (n_id_end>=0)
				{
					cur_node = p_tree->listNeuron.at(n_id_end);
					qDebug()<<cur_node.x<<" "<<cur_node.y<<" "<<cur_node.z;
					XYZ cur_node_xyz = XYZ(cur_node.x, cur_node.y, cur_node.z);
					if (dist_L2(cur_node_xyz, loc_vec.at(N-1))<th_merge)
					{
						loc_vec.at(N-1) = cur_node_xyz;
						b_end_merged = true;
						qDebug()<<"force set the last point of this curve to the above neuron node as they are close.";

					}
				}

				//a special operation is that if the end point is merged, but the start point is not merged,
				//then this segment is reversed direction to reflect the prior knowledge that a neuron normally grow out as branches
				if (b_start_merged==false && b_end_merged==true)
				{
					vector <XYZ> loc_vec_tmp = loc_vec;
					for (int i=0;i<N;i++)
						loc_vec.at(i) = loc_vec_tmp.at(N-1-i);
				}
			}
		}
	}

	if (b_use_seriespointclick==false)
		smooth_curve(loc_vec, 5);
#endif
     if (b_addthiscurve)
     {
          addCurveSWC(loc_vec, chno);
          // used to convert loc_vec to NeuronTree and save SWC in testing
          vecToNeuronTree(testNeuronTree, loc_vec);
     }
     else //100821
     {
          b_addthiscurve = true; //in this case, always reset to default to draw curve to add to a swc instead of just  zoom
          endSelectMode();
     }
}


/**
 * @brief This function is used for getting loc using center of mass method.
 *  ZJL 2012-01-26
*/
XYZ Renderer_gl1::getLocUsingMassCenter(bool firstloc, XYZ lastpos, XYZ P1, XYZ P2,
		double clipplane[4],	//clipplane==0 means no clip plane
		int chno,    			//must be a valid channel number
		float *p_value			//if p_value!=0, output value at center
	)
{
	if (renderMode==rmCrossSection)
	{
		return getPointOnSections(P1,P2, clipplane); //clip plane also is the F-plane
	}

	XYZ loc = (P1+P2)*0.5;

#ifndef test_main_cpp

	V3dR_GLWidget* w = (V3dR_GLWidget*)widget;
	My4DImage* curImg = v3dr_getImage4d(_idep);

     // get signal statistics
     double signal_last, signal_cur, diff_min, diff_cur;
     LocationSimple pt, pt_cur;
     XYZ loc_min; //loc with min diff

     if(!firstloc)
     {
          getRgnPropertyAt(lastpos, pt);
          signal_last=pt.ave + pt.sdev;

          getRgnPropertyAt(loc, pt_cur);
          signal_cur=pt_cur.ave + pt_cur.sdev;

          diff_min = abs( signal_last-signal_cur );
          loc_min = loc;
     }

	if (curImg && data4dp && chno>=0 &&  chno <dim4)
	{
		double f = 0.8; // must be LESS 1 to converge, close to 1 is better

		XYZ D = P2-P1; normalize(D);

		unsigned char* vp = 0;
		switch (curImg->getDatatype())
		{
			case V3D_UINT8:
				vp = data4dp + (chno + volTimePoint*dim4)*(dim3*dim2*dim1);
				break;
			case V3D_UINT16:
				vp = data4dp + (chno + volTimePoint*dim4)*(dim3*dim2*dim1)*sizeof(short int);
				break;
			case V3D_FLOAT32:
				vp = data4dp + (chno + volTimePoint*dim4)*(dim3*dim2*dim1)*sizeof(float);
				break;
			default:
				v3d_msg("Unsupported data type found. You should never see this.", 0);
                    if(!firstloc)
                         return loc_min;
                    else
                         return loc;
		}

		for (int i=0; i<200; i++) // iteration, (1/f)^200 is big enough
		{
			double length = norm(P2-P1);
			if (length < 0.5) // pixel
				break; //////////////////////////////////

			int nstep = int(length + 0.5);
			double step = length/nstep;

			XYZ sumloc(0,0,0);
			float sum = 0;
			for (int i=0; i<=nstep; i++)
			{
				XYZ P = P1 + D*step*(i);
				float value;
				switch (curImg->getDatatype())
				{
					case V3D_UINT8:
						value = sampling3dAllTypesatBounding( vp, dim1, dim2, dim3,  P.x, P.y, P.z, dataViewProcBox.box, clipplane);
						break;
					case V3D_UINT16:
						value = sampling3dAllTypesatBounding( (short int *)vp, dim1, dim2, dim3,  P.x, P.y, P.z, dataViewProcBox.box, clipplane);
						break;
					case V3D_FLOAT32:
						value = sampling3dAllTypesatBounding( (float *)vp, dim1, dim2, dim3,  P.x, P.y, P.z, dataViewProcBox.box, clipplane);
						break;
					default:
						v3d_msg("Unsupported data type found. You should never see this.", 0);
						if(!firstloc)
                                   return loc_min;
                              else
                                   return loc;
				}

				sumloc = sumloc + P*(value);
				sum = sum + value;
			}

			if (sum)
				loc = sumloc / sum;
			else
				break; //////////////////////////////////

			P1 = loc - D*(length*f/2);
			P2 = loc + D*(length*f/2);

               // get statistical infor on loc and compare with previous one
               if(!firstloc)
               {
                    getRgnPropertyAt(loc, pt_cur);
                    signal_cur=pt_cur.ave+pt_cur.sdev;

                    diff_cur = abs( signal_cur-signal_last );
                    if(diff_cur<diff_min)
                    {
                         loc_min=loc;
                         diff_min=diff_cur;
                    }
               }

		}


		if (p_value)
		{
			XYZ P = loc;
			float value;
			switch (curImg->getDatatype())
			{
				case V3D_UINT8:
					value = sampling3dAllTypesatBounding( vp, dim1, dim2, dim3,  P.x, P.y, P.z, dataViewProcBox.box, clipplane);
					break;
				case V3D_UINT16:
					value = sampling3dAllTypesatBounding( (short int *)vp, dim1, dim2, dim3,  P.x, P.y, P.z, dataViewProcBox.box, clipplane);
					break;
				case V3D_FLOAT32:
					value = sampling3dAllTypesatBounding( (float *)vp, dim1, dim2, dim3,  P.x, P.y, P.z, dataViewProcBox.box, clipplane);
					break;
				default:
					v3d_msg("Unsupported data type found. You should never see this.", 0);
					return loc;
			}
			*p_value = value;
		}

	}//valid data
#endif

	//100721: Now small tag added in sampling3dUINT8atBounding makes loc in bound box
     if(!firstloc)
          return loc_min;
     else
          return loc;

}

/*
 @brief Calculate the line segment PaPb that is the shortest route between
 two lines P1P2 and P3P4. Calculate also the values of mua and mub where
 Pa = P1 + mua (P2 - P1)
 Pb = P3 + mub (P4 - P3)
 Return FALSE if no solution exists.
http://paulbourke.net/geometry/lineline3d/
 */
bool Renderer_gl1::lineLineIntersect( XYZ p1,XYZ p2,XYZ p3,XYZ p4,XYZ *pa,XYZ *pb,
					  double *mua, double *mub)
{
	XYZ p13,p43,p21;
	double d1343,d4321,d1321,d4343,d2121;
	double numer,denom;

	p13.x = p1.x - p3.x;
	p13.y = p1.y - p3.y;
	p13.z = p1.z - p3.z;
	p43.x = p4.x - p3.x;
	p43.y = p4.y - p3.y;
	p43.z = p4.z - p3.z;
	if (abs(p43.x) < EPS && abs(p43.y) < EPS && abs(p43.z) < EPS)
		return false;
	p21.x = p2.x - p1.x;
	p21.y = p2.y - p1.y;
	p21.z = p2.z - p1.z;
	if (abs(p21.x) < EPS && abs(p21.y) < EPS && abs(p21.z) < EPS)
		return false;

	d1343 = p13.x * p43.x + p13.y * p43.y + p13.z * p43.z;
	d4321 = p43.x * p21.x + p43.y * p21.y + p43.z * p21.z;
	d1321 = p13.x * p21.x + p13.y * p21.y + p13.z * p21.z;
	d4343 = p43.x * p43.x + p43.y * p43.y + p43.z * p43.z;
	d2121 = p21.x * p21.x + p21.y * p21.y + p21.z * p21.z;

	denom = d2121 * d4343 - d4321 * d4321;
	if (abs(denom) < EPS)
		return false;
	numer = d1343 * d4321 - d1321 * d4343;

	*mua = numer / denom;
	*mub = (d1343 + d4321 * (*mua)) / d4343;

	pa->x = p1.x + *mua * p21.x;
	pa->y = p1.y + *mua * p21.y;
	pa->z = p1.z + *mua * p21.z;
	pb->x = p3.x + *mub * p43.x;
	pb->y = p3.y + *mub * p43.y;
	pb->z = p3.z + *mub * p43.z;

	return true;
}

/*
 *@brief Chech whether point pa is on the line (p1,p2) and within (p1,p2)
*/
bool Renderer_gl1::withinLineSegCheck( XYZ p1,XYZ p2,XYZ pa)
{
     XYZ p12 = p2-p1;
     XYZ p1a = pa-p1;
     bool colinear = norm(cross(p12, p1a))<EPS ;
     double dotpro = dot(p12, p1a);
     double dot12 = dot(p12, p12); // square length between 12
     bool within = (dotpro>0 && dotpro<dot12);

     return (colinear && within);
}

// get the control points of the primary curve

void Renderer_gl1::getSubVolInfo(XYZ lastloc, XYZ loc0, XYZ loc1, XYZ &sub_orig, double* &pSubdata,
                                V3DLONG &sub_szx, V3DLONG &sub_szy, V3DLONG &sub_szz)
{
     //
     int boundary = 10;
     XYZ minloc, maxloc;
     minloc.x = MIN(lastloc.x, MIN(loc0.x, loc1.x)) - boundary;
     minloc.y = MIN(lastloc.y, MIN(loc0.y, loc1.y)) - boundary;
     minloc.z = MIN(lastloc.z, MIN(loc0.z, loc1.z)) - boundary;

     maxloc.x = MAX(lastloc.x, MAX(loc0.x, loc1.x)) + boundary;
     maxloc.y = MAX(lastloc.y, MAX(loc0.y, loc1.y)) + boundary;
     maxloc.z = MAX(lastloc.z, MAX(loc0.z, loc1.z)) + boundary;

     V3dR_GLWidget* w = (V3dR_GLWidget*)widget;
     My4DImage* curImg = 0;
     if (w)
          curImg = v3dr_getImage4d(_idep);


     V3DLONG szx = curImg->getXDim();
     V3DLONG szy = curImg->getYDim();
     V3DLONG szz = curImg->getZDim();
     minloc.x = qBound((V3DLONG)0, (V3DLONG)minloc.x, (V3DLONG)(szx-1));
     minloc.y = qBound((V3DLONG)0, (V3DLONG)minloc.y, (V3DLONG)(szy-1));
     minloc.z = qBound((V3DLONG)0, (V3DLONG)minloc.z, (V3DLONG)(szz-1));

     maxloc.x = qBound((V3DLONG)0, (V3DLONG)maxloc.x, (V3DLONG)(szx-1));
     maxloc.y = qBound((V3DLONG)0, (V3DLONG)maxloc.y, (V3DLONG)(szy-1));
     maxloc.z = qBound((V3DLONG)0, (V3DLONG)maxloc.z, (V3DLONG)(szz-1));

     // The data is from minloc to maxloc
     sub_szx=maxloc.x-minloc.x+1;
     sub_szy=maxloc.y-minloc.y+1;
     sub_szz=maxloc.z-minloc.z+1;

     sub_orig = minloc;

     pSubdata = new double [sub_szx*sub_szy*sub_szz];
     for(V3DLONG k=0; k<sub_szz; k++)
          for(V3DLONG j=0; j<sub_szy; j++)
               for(V3DLONG i=0; i<sub_szx; i++)
               {
                    V3DLONG ind = k*sub_szy*sub_szx + j*sub_szx + i;
                    pSubdata[ind]=curImg->at(i,j,k);
               }


}


void Renderer_gl1::solveCurveFromMarkersFastMarching()
{
	qDebug("  Renderer_gl1::solveCurveMarkersFastMarching");

	vector <XYZ> loc_vec_input;
     loc_vec_input.clear();

	V3dR_GLWidget* w = (V3dR_GLWidget*)widget;
	My4DImage* curImg = 0;
     if (w) curImg = v3dr_getImage4d(_idep); //by PHC, 090119

      int chno = checkCurChannel();
          if (chno<0 || chno>dim4-1)   chno = 0; //default first channel

     if (selectMode == smCurveCreate_pointclick_fm)
     {
          if (curImg)
          {
               curImg->update_3drenderer_neuron_view(w, this);
               QList <LocationSimple> & listLoc = curImg->listLandmarks;
               int spos = listLoc.size()-cntCur3DCurveMarkers;
               XYZ cur_xyz;
               int i;
               for (i=spos;i<listLoc.size();i++)
               {
                    cur_xyz.x = listLoc.at(i).x-1; //marker location is 1 based
                    cur_xyz.y = listLoc.at(i).y-1;
                    cur_xyz.z = listLoc.at(i).z-1;
                    loc_vec_input.push_back(cur_xyz);
               }
               for (i=listLoc.size()-1;i>=spos;i--)
               {
                    listLoc.removeLast();
               }
               updateLandmark();
          }
     }
     else if(selectMode == smCurveMarkerPool_fm)
     {
          int nn=listCurveMarkerPool.size();
          XYZ cur_xyz;
          int i;
          for (i=0;i<nn;i++)
          {
               cur_xyz.x = listCurveMarkerPool.at(i).x-1; //marker location is 1 based
               cur_xyz.y = listCurveMarkerPool.at(i).y-1;
               cur_xyz.z = listCurveMarkerPool.at(i).z-1;
               loc_vec_input.push_back(cur_xyz);
          }
     }

     // loc_vec is used to store final locs on the curve
     vector <XYZ> loc_vec;
     loc_vec.clear();

     // vec used for second fastmarching
     vector<XYZ> middle_vec;
     middle_vec.clear();

     // set pregress dialog
     PROGRESS_DIALOG( "Curve creating", widget);
     PROGRESS_PERCENT(10);

     if (loc_vec_input.size()>0)
	{
          middle_vec.push_back(loc_vec_input.front());//first marker
          loc_vec.push_back(loc_vec_input.front());

          V3DLONG szx = curImg->getXDim();
          V3DLONG szy = curImg->getYDim();
          V3DLONG szz = curImg->getZDim();

          // get img data pointer
          unsigned char* pImg = 0;
          if (curImg && data4dp && chno>=0 &&  chno <dim4)
          {
               switch (curImg->getDatatype())
               {
                    case V3D_UINT8:
                         pImg = data4dp + (chno + volTimePoint*dim4)*(dim3*dim2*dim1);
                         break;
                    case V3D_UINT16:
                         pImg = data4dp + (chno + volTimePoint*dim4)*(dim3*dim2*dim1)*sizeof(short int);
                         break;
                    case V3D_FLOAT32:
                         pImg = data4dp + (chno + volTimePoint*dim4)*(dim3*dim2*dim1)*sizeof(float);
                         break;
                    default:
                         v3d_msg("Unsupported data type found. You should never see this.", 0);
                         return ;
               }
          }else
          {
               v3d_msg("No data available. You should never see this.", 0);
               return ;
          }

		qDebug("now get curve using fastmarching method");
          // Using fast_marching method to get loc
          for(int ii=0; ii<loc_vec_input.size()-1; ii++)
          {
               XYZ loc0=loc_vec_input.at(ii);
               XYZ loc1=loc_vec_input.at(ii+1);

               vector<MyMarker*> outswc;

               vector<MyMarker> sub_markers;
               vector<MyMarker> tar_markers;
               MyMarker mloc0(loc0.x, loc0.y, loc0.z);
               MyMarker mloc1(loc1.x, loc1.y, loc1.z);
               sub_markers.push_back(mloc0);
               tar_markers.push_back(mloc1);

               // call fastmarching
               fastmarching_linker(sub_markers, tar_markers, pImg, outswc, szx, szy, szz, 0);

               if(!outswc.empty())
               {
                    // the 1st loc in outswc is the last pos got in fm
                    for(int j=outswc.size()-2; j>=0; j-- )
                    {
                         XYZ loc;
                         loc.x=outswc.at(j)->x;
                         loc.y=outswc.at(j)->y;
                         loc.z=outswc.at(j)->z;

                         loc_vec.push_back(loc);
                         if(j==outswc.size()/2) // save middle point
                         {
                              middle_vec.push_back(loc);
                         }
                    }

                   //always remember to free the potential-memory-problematic fastmarching_linker return value
                   clean_fm_marker_vector(outswc);

               }else
               {
                    loc_vec.push_back(loc1);
               }

          }

          loc_vec.push_back(loc_vec_input.back());

          PROGRESS_PERCENT(60);

          // append the last XYZ of loc_vec to middle_vec
          middle_vec.push_back(loc_vec_input.back());// last marker

          //===============================================================================>>>>>>>>>>>>
          int N = loc_vec.size();
          if(N<1) return; // all points are outside the volume. ZJL 110913

          loc_vec.clear();

          loc_vec.push_back(middle_vec.at(0)); //append the first loc
          // Do the second fastmarching for smoothing the curve
          for(int jj=0;jj<middle_vec.size()-1; jj++)
          {
               // create MyMarker list
               vector<MyMarker> sub_markers;
               vector<MyMarker> tar_markers;
               vector<MyMarker*> outswc;
               XYZ loc;

               // sub_markers
               XYZ loc0=middle_vec.at(jj);
               MyMarker mloc0 = MyMarker(loc0.x, loc0.y, loc0.z);
               sub_markers.push_back(mloc0);

               // tar_markers
               XYZ loc1=middle_vec.at(jj+1);
               MyMarker mloc1 = MyMarker(loc1.x, loc1.y, loc1.z);
               tar_markers.push_back(mloc1);

               // call fastmarching
               fastmarching_linker(sub_markers, tar_markers, pImg, outswc, szx, szy, szz, 0.0);// time_thresh);

               if(!outswc.empty())
               {
                    // the 1st loc in outswc is the last pos got in fm
                    int nn = outswc.size();
                    for(int j=nn-2; j>=0; j-- )
                    {
                         XYZ locj;
                         locj.x=outswc.at(j)->x;
                         locj.y=outswc.at(j)->y;
                         locj.z=outswc.at(j)->z;

                         loc_vec.push_back(locj);
                    }
                    //always remember to free the potential-memory-problematic fastmarching_linker return value
                    clean_fm_marker_vector(outswc);
               }
               else
               {
                    loc_vec.push_back(loc1);
               }

          } // end for the second fastmarching
          loc_vec.push_back(middle_vec.back());
          //===============================================================================<<<<<<<<<<<<<
          PROGRESS_PERCENT(80);

          if(loc_vec.size()<1) return; // all points are outside the volume. ZJL 110913

          // check if there is any existing neuron node is very close to the starting and ending points, if yes, then merge

          MainWindow* V3Dmainwindow = 0;
          V3Dmainwindow = v3dr_getV3Dmainwindow(_idep);

          N = loc_vec.size();

          if (V3Dmainwindow && V3Dmainwindow->global_setting.b_3dcurve_autoconnecttips)
          {
               if (listNeuronTree.size()>0 && curEditingNeuron>0 && curEditingNeuron<=listNeuronTree.size())
               {
                    NeuronTree *p_tree = (NeuronTree *)(&(listNeuronTree.at(curEditingNeuron-1)));
                    if (p_tree)
                    {
                         V3DLONG n_id_start = findNearestNeuronNode_Loc(loc_vec.at(0), p_tree);
                         V3DLONG n_id_end = findNearestNeuronNode_Loc(loc_vec.at(N-1), p_tree);
                         qDebug("detect nearest neuron node [%ld] for curve-start and node [%ld] for curve-end for the [%d] neuron", n_id_start, n_id_end, curEditingNeuron);

                         double th_merge = 5;

                         bool b_start_merged=false, b_end_merged=false;
                         NeuronSWC cur_node;
                         if (n_id_start>=0)
                         {
                              cur_node = p_tree->listNeuron.at(n_id_start);
                              qDebug()<<cur_node.x<<" "<<cur_node.y<<" "<<cur_node.z;
                              XYZ cur_node_xyz = XYZ(cur_node.x, cur_node.y, cur_node.z);
                              if (dist_L2(cur_node_xyz, loc_vec.at(0))<th_merge)
                              {
                                   loc_vec.at(0) = cur_node_xyz;
                                   b_start_merged = true;
                                   qDebug()<<"force set the first point of this curve to the above neuron node as they are close.";
                              }
                         }
                         if (n_id_end>=0)
                         {
                              cur_node = p_tree->listNeuron.at(n_id_end);
                              qDebug()<<cur_node.x<<" "<<cur_node.y<<" "<<cur_node.z;
                              XYZ cur_node_xyz = XYZ(cur_node.x, cur_node.y, cur_node.z);
                              if (dist_L2(cur_node_xyz, loc_vec.at(N-1))<th_merge)
                              {
                                   loc_vec.at(N-1) = cur_node_xyz;
                                   b_end_merged = true;
                                   qDebug()<<"force set the last point of this curve to the above neuron node as they are close.";

                              }
                         }

                         //a special operation is that if the end point is merged, but the start point is not merged,
                         //then this segment is reversed direction to reflect the prior knowledge that a neuron normally grow out as branches
                         if (b_start_merged==false && b_end_merged==true)
                         {
                              vector <XYZ> loc_vec_tmp = loc_vec;
                              for (int i=0;i<N;i++)
                                   loc_vec.at(i) = loc_vec_tmp.at(N-1-i);
                         }
                    }
               }
          }

          smooth_curve(loc_vec, 5);

          // adaptive curve simpling
          vector <XYZ> loc_vec_resampled;
          int stepsize = 6; // sampling stepsize
          loc_vec_resampled.clear();
          adaptiveCurveResampling(loc_vec, loc_vec_resampled, stepsize);

          if (b_addthiscurve)
          {
               addCurveSWC(loc_vec_resampled, chno);
               // used to convert loc_vec to NeuronTree and save SWC in testing
               vecToNeuronTree(testNeuronTree, loc_vec);
          }
          else //100821
          {
               b_addthiscurve = true; //in this case, always reset to default to draw curve to add to a swc instead of just  zoom
               endSelectMode();
          }
     }
}
/**
 * @brief The curve is resampled based on its local curvature:
 *  L1(t-1,t), L2(t,t+1). If the angle between L1 and L2 is less than thresh_theta1. sample in stepsize.
 *  If the angle is between thresh_theta1 and thresh_theta2, sample in smaller stepsize.
 *  Otherwise, using original resolution.
 *  Jianong Zhou 20120204
*/
void Renderer_gl1::adaptiveCurveResampling(vector <XYZ> &loc_vec, vector <XYZ> &loc_vec_resampled, int stepsize)
{
     int N = loc_vec.size();
     loc_vec_resampled.clear();
     loc_vec_resampled.push_back(loc_vec.at(0));

     // used to control whether cur-to-nex locs are added
     bool b_prestep_added = false;

     for(int i=stepsize; i<N; i=i+stepsize)
     {
          XYZ loc_cur, loc_pre, loc_nex;
          int ind_nex = ( (i+stepsize)>=N )? (N-1):(i+stepsize);
          loc_cur = loc_vec.at(i);
          loc_pre = loc_vec.at(i-stepsize);
          loc_nex = loc_vec.at(ind_nex);

          XYZ v1 = loc_cur-loc_pre;
          XYZ v2 = loc_nex-loc_cur;

          // check the angle betwen v1 and v2
          float cos_theta;
          normalize(v1); normalize(v2);
          float theta = acos( dot(v1,v2) ) * 180.0/PI; // radius to degree

          // threshold theta
          float thresh_theta1 = 30.0;
          float thresh_theta2 = 60.0;

          if(theta<thresh_theta1)
          {
               loc_vec_resampled.push_back(loc_vec.at(i));
               b_prestep_added = false;
          }
          else if( (theta>=thresh_theta1) && (theta<=thresh_theta2) )
          {
               float new_step = stepsize/2;
               int ind_start;
               if(b_prestep_added)
                    ind_start = i;
               else
                    ind_start = i-stepsize;

               for( int j=ind_start+new_step; j<=ind_nex; j=j+new_step)
               {
                    loc_vec_resampled.push_back( loc_vec.at(j) );
               }
               b_prestep_added = true;

          }
          else if( theta>thresh_theta2 )
          {
               int ind_start;
               if(b_prestep_added)
                    ind_start = i;
               else
                    ind_start = i-stepsize;

               for(int j=ind_start+1; j<=ind_nex; j++)
               {
                    loc_vec_resampled.push_back( loc_vec.at(j) );
               }

               b_prestep_added = true;
          }
     }
     loc_vec_resampled.push_back(loc_vec.back());
}


/**
 * @brief This function is based on findNearestNeuronNode_WinXY(int cx, int cy,
 *  NeuronTree* ptree).
 */
V3DLONG Renderer_gl1::findNearestNeuronNode_Loc(XYZ &loc, NeuronTree *ptree)
{
	if (!ptree) return -1;
	QList <NeuronSWC> *p_listneuron = &(ptree->listNeuron);
	if (!p_listneuron) return -1;

	//qDebug()<<"win click position:"<<cx<<" "<<cy;
     GLdouble px, py, pz, ix, iy, iz;

	V3DLONG best_ind=-1; double best_dist=-1;
	for (V3DLONG i=0;i<p_listneuron->size();i++)
	{
		ix = p_listneuron->at(i).x, iy = p_listneuron->at(i).y, iz = p_listneuron->at(i).z;

		double cur_dist = (loc.x-ix)*(loc.x-ix)+(loc.y-iy)*(loc.y-iy)+(loc.z-iz)*(loc.z-iz);
		if (i==0) {	best_dist = cur_dist; best_ind=0; }
		else {	if (cur_dist<best_dist) {best_dist=cur_dist; best_ind = i;}}
	}

	return best_ind; //by PHC, 090209. return the index in the SWC file
}


void Renderer_gl1::solveCurveMarkerLists_fm(vector <XYZ> & loc_vec_input, vector <XYZ> &loc_vec, int index)
{
	bool b_use_seriespointclick = (loc_vec_input.size()>0) ? true : false;
     if (b_use_seriespointclick==false && list_listCurvePos.size()<1)  return;

	bool b_use_last_approximate=true;

	V3dR_GLWidget* w = (V3dR_GLWidget*)widget;
	My4DImage* curImg = 0;
     if (w) curImg = v3dr_getImage4d(_idep); //by PHC, 090119

#ifndef test_main_cpp
	MainWindow* V3Dmainwindow = 0;
	V3Dmainwindow = v3dr_getV3Dmainwindow(_idep);
	if (V3Dmainwindow)
		b_use_last_approximate = V3Dmainwindow->global_setting.b_3dcurve_inertia;
#endif

	int chno = checkCurChannel();
	if (chno<0 || chno>dim4-1)   chno = 0; //default first channel
	qDebug()<<"\n solveCurveMarkerLists: 3d curve in channel # "<<((chno<0)? chno :chno+1);

	loc_vec.clear();

     // add intersection point into listMarker
     listMarker.clear();
     int totalm=0;//total intersection point

     // vec used for second fastmarching
     vector<XYZ> middle_vec;
     middle_vec.clear();

     V3DLONG szx = curImg->getXDim();
     V3DLONG szy = curImg->getYDim();
     V3DLONG szz = curImg->getZDim();

     // set pregress dialog
     PROGRESS_DIALOG( "Curve creating", widget);
     PROGRESS_PERCENT(10);

     // get img data pointer for fastmarching
     unsigned char* pImg = 0;
     if (curImg && data4dp && chno>=0 &&  chno <dim4)
     {
          switch (curImg->getDatatype())
          {
               case V3D_UINT8:
                    pImg = data4dp + (chno + volTimePoint*dim4)*(dim3*dim2*dim1);
                    break;
               case V3D_UINT16:
                    pImg = data4dp + (chno + volTimePoint*dim4)*(dim3*dim2*dim1)*sizeof(short int);
                    break;
               case V3D_FLOAT32:
                    pImg = data4dp + (chno + volTimePoint*dim4)*(dim3*dim2*dim1)*sizeof(float);
                    break;
               default:
                    v3d_msg("Unsupported data type found. You should never see this.", 0);
                    return ;
          }
     }else
     {
          v3d_msg("No data available. You should never see this.", 0);
          return ;
     }

	int N = loc_vec_input.size();
	if (b_use_seriespointclick)
	{
		loc_vec = loc_vec_input;
	}
	else //then use the moving mouse location, otherwise using the preset loc_vec_input (which is set by the 3d-curve-by-point-click function)
	{
		N = list_listCurvePos.at(index).size(); // change from 0 to index for different curves, ZJL

          // resample curve strokes
          vector<int> inds; // reserved stroke index
          resampleCurveStrokes(0, chno, inds);

		for (int i=0; i<N; i++)
		{
               // check whether i is in inds
               bool b_inds=false;
               if(inds.empty())
               {
                    b_inds=true;
               }
               else
               {
                    for(int ii=0; ii<inds.size(); ii++)
                    {
                         if(i == inds.at(ii))
                         {
                              b_inds=true;
                              break;
                         }
                    }
               }

               // only process resampled strokes
               if(i==0 || i==(N-1) || b_inds)
               {
                    const MarkerPos & pos = list_listCurvePos.at(index).at(i); // change from 0 to index for different curves, ZJL

                    //100730 RZC, in View space, keep for dot(clip, pos)>=0
                    double clipplane[4] = { 0.0,  0.0, -1.0,  0 };
                    clipplane[3] = viewClip;
                    ViewPlaneToModel(pos.MV, clipplane);

                    XYZ loc0, loc1;
                    _MarkerPos_to_NearFarPoint(pos, loc0, loc1);

                    // prepare the first sub_markers used in fm
                    // vector<MyMarker> sub_markers_1st;
                    // if(i==0)
                    // {
                    //      XYZ v_1_0_1st = loc1-loc0;
                    //      XYZ D_1st = v_1_0_1st; normalize(D_1st);
                    //      float length_1st = dist_L2(loc0, loc1);

                    //      for(int ii=0; ii<=(int)(length_1st+0.5); ii++)
                    //      {
                    //           XYZ loci = loc0 + D_1st*ii; // incease 1 each step
                    //           MyMarker mloci = MyMarker(loci.x, loci.y, loci.z);
                    //           sub_markers_1st.push_back(mloci);
                    //      }
                    // }


                    XYZ loc;
                    float length01 = dist_L2(loc0, loc1);
                    int last_j = loc_vec.size()-1;
                    XYZ lastpos;
                    if(last_j<0) //i==0
                    {
                         // always push_back the 1st loc
                         //loc = getCenterOfLineProfile(loc0, loc1, clipplane, chno);
                         // get the loc with a random middle loc
                         getMidRandomLoc(pos, chno, loc);
                         middle_vec.push_back(loc);
                    }
                    else if (last_j>=0)
                    {
                         lastpos = loc_vec.at(last_j);
                         if(b_use_last_approximate) //091114 PHC make it more stable by conditioned on previous location
                         {
                              if (dataViewProcBox.isInner(lastpos, 0.5))
                              {
                                   XYZ v_1_0 = loc1-loc0, v_0_last=loc0-lastpos;
                                   XYZ nearestloc = loc0-v_1_0*dot(v_0_last, v_1_0)/dot(v_1_0, v_1_0); //since loc0!=loc1, this is safe
                                   double ranget = (length01/2.0)>10?10:(length01/2.0); //at most 30 pixels aparts
                                   XYZ D = v_1_0; normalize(D);
                                   loc0 = nearestloc - D*(ranget);
                                   loc1 = nearestloc + D*(ranget);
                              }
                         }


                         // use fastmarching to create curve between (lastloc0,lastloc1) and (loc0,loc1)
                         // create MyMarker list
                         vector<MyMarker> sub_markers;
                         vector<MyMarker> tar_markers;
                         vector<MyMarker*> outswc;

                         // for sub_markers
                         // if(last_j<0 && i!=0)
                         // {
                         //      sub_markers = sub_markers_1st; //the first point
                         // }
                         // else
                         {
                              // sub_markers is the lastpos
                              MyMarker mlastloc = MyMarker(lastpos.x, lastpos.y, lastpos.z);
                              sub_markers.push_back(mlastloc);
                         }


                         float length = dist_L2(loc0, loc1);
                         if (length<1.0)
                         {
                              // if(i == N-1) // last point
                              // {
                              //      int range=6;
                              //      srand(clock());
                              //      MarkerPos pos_temp = pos;
                              //      for(int j=0; j<2*range; j++)
                              //      {
                              //           // generate pos and then get new loc
                              //           int rand_x = rand()%(range+1); // generate value from 0~range
                              //           int rand_y = rand()%(range+1); // generate value from 0~range
                              //           // map rand from 0~range to -range/2~range/2
                              //           rand_x = -range/2+rand_x;
                              //           rand_y = -range/2+rand_y;
                              //           pos_temp.x = pos.x + rand_x;
                              //           pos_temp.y = pos.y + rand_y;

                              //           _MarkerPos_to_NearFarPoint(pos_temp, loc0, loc1);
                              //           XYZ v_1_0 = loc1-loc0;
                              //           XYZ D = v_1_0; normalize(D);

                              //           for(int ii=0; ii<(int)(length+0.5); ii++)
                              //           {
                              //                XYZ loci = loc0 + D*ii; // incease 1 each step
                              //                MyMarker mloci = MyMarker(loci.x, loci.y, loci.z);
                              //                tar_markers.push_back(mloci);
                              //           }
                              //      }
                              // }
                              // else
                              {
                                   XYZ loci=(loc0+loc1)/2.0;
                                   MyMarker mloci = MyMarker(loci.x, loci.y, loci.z);
                                   tar_markers.push_back(mloci);
                              }


                              // //add neighbors of loci
                              // vector<XYZ> neibs_loci;
                              // neibs_loci.clear();
                              // INSERT_NEIGHBOR(XYZ(loci.x+1, loci.y, loci.z));
                              // INSERT_NEIGHBOR(XYZ(loci.x-1, loci.y, loci.z));
                              // INSERT_NEIGHBOR(XYZ(loci.x, loci.y+1, loci.z));
                              // INSERT_NEIGHBOR(XYZ(loci.x, loci.y-1, loci.z));
                              // INSERT_NEIGHBOR(XYZ(loci.x, loci.y, loci.z+1));
                              // INSERT_NEIGHBOR(XYZ(loci.x, loci.y, loci.z-1));

                              // set <V3DLONG> neibs_set;
                              // for(int jj=0;jj<neibs_loci.size();jj++)
                              // {
                              //      XYZ locj = neibs_loci.at(jj);
                              //      V3DLONG locj1d = locj.z*szx*szy + locj.y*szx + locj.x;
                              //      neibs_set.insert(locj1d);
                              // }

                              // set<V3DLONG>::iterator it;
                              // for(it=neibs_set.begin(); it!=neibs_set.end(); it++)
                              // {
                              //      XYZ locj;
                              //      V3DLONG locj1d=*it;
                              //      locj.x=locj1d % szx;
                              //      locj.y=((locj1d-(int)locj.x)/szx) % szy;
                              //      locj.z=(locj1d-(int)locj.y*szx-(int)locj.x)/(szx*szy);
                              //      tar_markers.push_back(MyMarker(locj.x, locj.y, locj.z));
                              // }
                         }
                         else
                         {
                              if(i == N-1) // last point
                              {
                                   int range=6;
                                   srand(clock());
                                   MarkerPos pos_temp = pos;
                                   for(int j=0; j<2*range; j++)
                                   {
                                        // generate pos and then get new loc
                                        int rand_x = rand()%(range+1); // generate value from 0~range
                                        int rand_y = rand()%(range+1); // generate value from 0~range
                                        // map rand from 0~range to -range/2~range/2
                                        rand_x = -range/2+rand_x;
                                        rand_y = -range/2+rand_y;
                                        pos_temp.x = pos.x + rand_x;
                                        pos_temp.y = pos.y + rand_y;
                                        XYZ loc00, loc11;
                                        _MarkerPos_to_NearFarPoint(pos_temp, loc00, loc11);
                                        XYZ v_1_0 = loc11-loc00;
                                        XYZ D = v_1_0; normalize(D);

                                        if (dataViewProcBox.isInner(lastpos, 0.5))
                                        {
                                             float length0011 = dist_L2(loc00, loc11);
                                             XYZ v_0_last=loc00-lastpos;
                                             XYZ nearestloc = loc00-v_1_0*dot(v_0_last, v_1_0)/dot(v_1_0, v_1_0); //since loc0!=loc1, this is safe
                                             double ranget = (length0011/2.0)>10?10:(length0011/2.0); //at most 30 pixels aparts
                                             D = v_1_0; normalize(D);
                                             loc00 = nearestloc - D*(ranget);
                                             loc11 = nearestloc + D*(ranget);
                                        }

                                        length = dist_L2(loc00,loc11);
                                        for(int ii=0; ii<(int)(length+0.5); ii++)
                                        {
                                             XYZ loci = loc00 + D*ii; // incease 1 each step
                                             MyMarker mloci = MyMarker(loci.x, loci.y, loci.z);
                                             tar_markers.push_back(mloci);
                                        }
                                   }
                              }
                              else
                              {
                                   XYZ v_1_0 = loc1-loc0;
                                   XYZ D = v_1_0; normalize(D);
                                   for(int ii=0; ii<(int)(length+0.5); ii++)
                                   {
                                        XYZ loci = loc0 + D*ii; // incease 1 each step
                                        MyMarker mloci = MyMarker(loci.x, loci.y, loci.z);
                                        tar_markers.push_back(mloci);

                                        //
                                        //
                                        //      //add neighbors of loci
                                        //      vector<XYZ> neibs_loci;
                                        //      neibs_loci.clear();
                                        //      INSERT_NEIGHBOR(XYZ(loci.x+1, loci.y, loci.z));
                                        //      INSERT_NEIGHBOR(XYZ(loci.x-1, loci.y, loci.z));
                                        //      INSERT_NEIGHBOR(XYZ(loci.x, loci.y+1, loci.z));
                                        //      INSERT_NEIGHBOR(XYZ(loci.x, loci.y-1, loci.z));
                                        //      INSERT_NEIGHBOR(XYZ(loci.x, loci.y, loci.z+1));
                                        //      INSERT_NEIGHBOR(XYZ(loci.x, loci.y, loci.z-1));

                                        //      set <V3DLONG> neibs_set;
                                        //      for(int jj=0;jj<neibs_loci.size();jj++)
                                        //      {
                                        //           XYZ locj = neibs_loci.at(jj);
                                        //           V3DLONG locj1d = locj.z*szx*szy + locj.y*szx + locj.x;
                                        //           neibs_set.insert(locj1d);
                                        //      }

                                        //      set<V3DLONG>::iterator it;
                                        //      for(it=neibs_set.begin(); it!=neibs_set.end(); it++)
                                        //      {
                                        //           XYZ locj;
                                        //           V3DLONG locj1d=*it;
                                        //           locj.x=locj1d % szx;
                                        //           locj.y=((locj1d-(int)locj.x)/szx) % szy;
                                        //           locj.z=(locj1d-(int)locj.y*szx-(int)locj.x)/(szx*szy);
                                        //           tar_markers.push_back(MyMarker(locj.x, locj.y, locj.z));
                                        //      }
                                        //
                                   }
                              }
                         }

                         // waiting time threshold
                         float time_thresh = 0.2; //in seconds

                         // call fastmarching
                         // using time spent on each step to decide whether the tracing in this step is acceptable.
                         // if time is over time_thresh, then break and use center method
                         // I found that the result is not so good when using this time limit
                         fastmarching_linker(sub_markers, tar_markers, pImg, outswc, szx, szy, szz,0.0);// time_thresh);

                         if(!outswc.empty())
                         {
                              // the 1st loc in outswc is the last pos got in fm
                              int nn = outswc.size();
                              for(int j=nn-1; j>0; j-- )
                              {
                                   XYZ locj;
                                   locj.x=outswc.at(j)->x;
                                   locj.y=outswc.at(j)->y;
                                   locj.z=outswc.at(j)->z;

                                   loc_vec.push_back(locj);

                                   // record the middle loc in this seg for the second fast marching
                                   if((nn>6)&&(j==(int)(nn/2)))
                                   {
                                        middle_vec.push_back(locj);
                                   }
                              }
                              // the last one
                              loc.x = outswc.at(0)->x;
                              loc.y = outswc.at(0)->y;
                              loc.z = outswc.at(0)->z;
                         }
                         else
                         {
                              loc = getCenterOfLineProfile(loc0, loc1, clipplane, chno);
                         }
                         //always remember to free the potential-memory-problematic fastmarching_linker return value
                         clean_fm_marker_vector(outswc);
                    }

                    if (dataViewProcBox.isInner(loc, 0.5))
                    {
                         dataViewProcBox.clamp(loc);
                         loc_vec.push_back(loc);
                    }
               }

          }
     }
     PROGRESS_PERCENT(60);
     //===============================================================================>>>>>>>>>>>>
     // put the last element of loc_vec
     middle_vec.push_back(loc_vec.back());
     N = loc_vec.size();
     if(N<1) return; // all points are outside the volume. ZJL 110913

     // append the last XYZ of loc_vec to middle_vec
     middle_vec.push_back(loc_vec.at(loc_vec.size()-1));
     loc_vec.clear();

     loc_vec.push_back(middle_vec.at(0)); //append the first loc
     // Do the second fastmarching for smoothing the curve
     for(int jj=0;jj<middle_vec.size()-1; jj++)
     {
          // create MyMarker list
          vector<MyMarker> sub_markers;
          vector<MyMarker> tar_markers;
          vector<MyMarker*> outswc;
          XYZ loc;

          // sub_markers
          XYZ loc0=middle_vec.at(jj);
          MyMarker mloc0 = MyMarker(loc0.x, loc0.y, loc0.z);
          sub_markers.push_back(mloc0);

          // tar_markers
          XYZ loc1=middle_vec.at(jj+1);
          MyMarker mloc1 = MyMarker(loc1.x, loc1.y, loc1.z);
          tar_markers.push_back(mloc1);

          // call fastmarching
          bool res = fastmarching_linker(sub_markers, tar_markers, pImg, outswc, szx, szy, szz, 0.0);// time_thresh);
          if(!res)
          {
               loc = loc1;
          }
          else
          {
               if(!outswc.empty())
               {
                    // the 1st loc in outswc is the last pos got in fm
                    int nn = outswc.size();
                    for(int j=nn-1; j>0; j-- )
                    {
                         XYZ locj;
                         locj.x=outswc.at(j)->x;
                         locj.y=outswc.at(j)->y;
                         locj.z=outswc.at(j)->z;

                         loc_vec.push_back(locj);
                    }
                    // the last one
                    loc.x = outswc.at(0)->x;
                    loc.y = outswc.at(0)->y;
                    loc.z = outswc.at(0)->z;
               }
               else
               {
                    loc = loc1;
               }
          }
          //always remember to free the potential-memory-problematic fastmarching_linker return value
          clean_fm_marker_vector(outswc);


          if (dataViewProcBox.isInner(loc, 0.5))
          {
               dataViewProcBox.clamp(loc);
               loc_vec.push_back(loc);
          }
     } // end for the second fastmarching
     //===============================================================================<<<<<<<<<<<<<
     PROGRESS_PERCENT(90);

     N = loc_vec.size(); //100722 RZC
     if(N<1) return; // all points are outside the volume. ZJL 110913

#ifndef test_main_cpp
	// check if there is any existing neuron node is very close to the starting and ending points, if yes, then merge
	if (V3Dmainwindow && V3Dmainwindow->global_setting.b_3dcurve_autoconnecttips && b_use_seriespointclick==false && (selectMode == smCurveMarkerLists_fm || selectMode == smCurveRefine_fm) )
	{
		if (listNeuronTree.size()>0 && curEditingNeuron>0 && curEditingNeuron<=listNeuronTree.size())
		{
			NeuronTree *p_tree = (NeuronTree *)(&(listNeuronTree.at(curEditingNeuron-1)));
			if (p_tree)
			{
                    // at(0) to at(index) ZJL 110901
                    V3DLONG n_id_start = findNearestNeuronNode_Loc(loc_vec.at(0), p_tree);
                    V3DLONG n_id_end = findNearestNeuronNode_Loc(loc_vec.at(N-1), p_tree);
                    qDebug("detect nearest neuron node [%ld] for curve-start and node [%ld] for curve-end for the [%d] neuron", n_id_start, n_id_end, curEditingNeuron);

				double th_merge = 5;

				bool b_start_merged=false, b_end_merged=false;
				NeuronSWC cur_node;
				if (n_id_start>=0)
				{
					cur_node = p_tree->listNeuron.at(n_id_start);
					qDebug()<<cur_node.x<<" "<<cur_node.y<<" "<<cur_node.z;
					XYZ cur_node_xyz = XYZ(cur_node.x, cur_node.y, cur_node.z);
					if (dist_L2(cur_node_xyz, loc_vec.at(0))<th_merge)
					{
						loc_vec.at(0) = cur_node_xyz;
						b_start_merged = true;
						qDebug()<<"force set the first point of this curve to the above neuron node as they are close.";
					}
				}
				if (n_id_end>=0)
				{
					cur_node = p_tree->listNeuron.at(n_id_end);
					qDebug()<<cur_node.x<<" "<<cur_node.y<<" "<<cur_node.z;
					XYZ cur_node_xyz = XYZ(cur_node.x, cur_node.y, cur_node.z);
					if (dist_L2(cur_node_xyz, loc_vec.at(N-1))<th_merge)
					{
						loc_vec.at(N-1) = cur_node_xyz;
						b_end_merged = true;
						qDebug()<<"force set the last point of this curve to the above neuron node as they are close.";

					}
				}

				//a special operation is that if the end point is merged, but the start point is not merged,
				//then this segment is reversed direction to reflect the prior knowledge that a neuron normally grow out as branches
				if (b_start_merged==false && b_end_merged==true)
				{
					vector <XYZ> loc_vec_tmp = loc_vec;
					for (int i=0;i<N;i++)
						loc_vec.at(i) = loc_vec_tmp.at(N-1-i);
				}
			}
		}
	}

	if (b_use_seriespointclick==false)
		smooth_curve(loc_vec, 5);
#endif

     // adaptive curve simpling
     vector <XYZ> loc_vec_resampled;
     int stepsize = 6; // sampling stepsize
     loc_vec_resampled.clear();
     adaptiveCurveResampling(loc_vec, loc_vec_resampled, stepsize);

     if(selectMode == smCurveMarkerLists_fm || selectMode == smCurveRefine_fm)
     {
          N=loc_vec.size();
          int NS = list_listCurvePos.at(index).size();
          if(N>3*NS)
          {
               if (QMessageBox::question(0, "", "The created curve may not correct. Do you want to continue create curve?", QMessageBox::Yes, QMessageBox::No)
                    == QMessageBox::Yes)
               {
                    if (b_addthiscurve)
                    {
                         addCurveSWC(loc_vec, chno);
                         // used to convert loc_vec to NeuronTree and save SWC in testing
                         vecToNeuronTree(testNeuronTree, loc_vec);
                    }
                    else
                    {
                         b_addthiscurve = true;
                         endSelectMode();
                    }

               }
          }
          else
          {
               if (b_addthiscurve)
               {
                    addCurveSWC(loc_vec, chno);
                    // used to convert loc_vec to NeuronTree and save SWC in testing
                    vecToNeuronTree(testNeuronTree, loc_vec);
               }
               else
               {
                    b_addthiscurve = true; //in this case, always reset to default to draw curve to add to a swc instead of just  zoom
                    endSelectMode();
               }
          }
     }

}


/**
 * Resample curve strokes:
 * 1. compute max value on each stroke viewing direction, and get a vector of these max values
 * 2. sort this vector, and return stroke indexes that their max values are above 10%
*/
void  Renderer_gl1::resampleCurveStrokes(int index, int chno, vector<int> &ids)
{
     V3dR_GLWidget* w = (V3dR_GLWidget*)widget;
     My4DImage* curImg = 0;
     if (w)
          curImg = v3dr_getImage4d(_idep);

	int N = list_listCurvePos.at(index).size();
     vector<double> maxval;
     maxval.clear();

     for (int i=0; i<N; i++)
     {
          const MarkerPos & pos = list_listCurvePos.at(index).at(i); // change from 0 to index for different curves, ZJL
          ////////////////////////////////////////////////////////////////////////
          //100730 RZC, in View space, keep for dot(clip, pos)>=0
          double clipplane[4] = { 0.0,  0.0, -1.0,  0 };
          clipplane[3] = viewClip;
          ViewPlaneToModel(pos.MV, clipplane);
          //qDebug()<<"   clipplane:"<<clipplane[0]<<clipplane[1]<<clipplane[2]<<clipplane[3];
          ////////////////////////////////////////////////////////////////////////

          XYZ loc0, loc1;
          _MarkerPos_to_NearFarPoint(pos, loc0, loc1);

          // get max value on each (loc0,loc)
          XYZ D = loc1-loc0; normalize(D);

		unsigned char* vp = 0;
		switch (curImg->getDatatype())
		{
			case V3D_UINT8:
				vp = data4dp + (chno + volTimePoint*dim4)*(dim3*dim2*dim1);
				break;
			case V3D_UINT16:
				vp = data4dp + (chno + volTimePoint*dim4)*(dim3*dim2*dim1)*sizeof(short int);
				break;
			case V3D_FLOAT32:
				vp = data4dp + (chno + volTimePoint*dim4)*(dim3*dim2*dim1)*sizeof(float);
				break;
			default:
				v3d_msg("Unsupported data type found. You should never see this.", 0);
				return;
		}

          // directly use max value on the line
          // double length = norm(loc1-loc0);
          // float maxvali=0.0;

          // double step;
          // int nstep = int(length + 0.5);
          // if(length<0.5)
          //      step = 1.0;
          // else
          //      step = length/nstep;

          // for (int i=0; i<=nstep; i++)
          // {
          //      XYZ P;
          //      if(length<0.5)
          //           P = (loc0+loc1)*0.5;
          //      else
          //           P= loc0 + D*step*(i);
          //      float value;
          //      switch (curImg->getDatatype())
          //      {
          //           case V3D_UINT8:
          //                value = sampling3dAllTypesatBounding( vp, dim1, dim2, dim3,  P.x, P.y, P.z, dataViewProcBox.box, clipplane);
          //                break;
          //           case V3D_UINT16:
          //                value = sampling3dAllTypesatBounding( (short int *)vp, dim1, dim2, dim3,  P.x, P.y, P.z, dataViewProcBox.box, clipplane);
          //                break;
          //           case V3D_FLOAT32:
          //                value = sampling3dAllTypesatBounding( (float *)vp, dim1, dim2, dim3,  P.x, P.y, P.z, dataViewProcBox.box, clipplane);
          //                break;
          //           default:
          //                v3d_msg("Unsupported data type found. You should never see this.", 0);
          //                return;
          //      }

          //      if(value > maxvali)
          //           maxvali = value;
          // }


          // use value on center of mass for comparing
          XYZ P  = getCenterOfLineProfile(loc0, loc1, clipplane, chno);
          float value;
          switch (curImg->getDatatype())
          {
               case V3D_UINT8:
                    value = sampling3dAllTypesatBounding( vp, dim1, dim2, dim3,  P.x, P.y, P.z, dataViewProcBox.box, clipplane);
                    break;
               case V3D_UINT16:
                    value = sampling3dAllTypesatBounding( (short int *)vp, dim1, dim2, dim3,  P.x, P.y, P.z, dataViewProcBox.box, clipplane);
                    break;
               case V3D_FLOAT32:
                    value = sampling3dAllTypesatBounding( (float *)vp, dim1, dim2, dim3,  P.x, P.y, P.z, dataViewProcBox.box, clipplane);
                    break;
               default:
                    v3d_msg("Unsupported data type found. You should never see this.", 0);
                    return;
          }
          float maxvali=value;

          // for two approaches
          maxval.push_back(maxvali);
     }

     // using map to sort maxval
     map<double, int> max_score;
     for(int i=0; i<maxval.size(); i++)
     {
          max_score[maxval.at(i)] = i;
     }

     map<double, int>::reverse_iterator it;

     int count=0;
     for(it=max_score.rbegin(); it!=max_score.rend(); it++)
     {
          count++;
          if(count >= max_score.size()/10.0)
               break;
          ids.push_back(it->second);
     }

}


void Renderer_gl1::solveCurveFromMarkersGD(bool b_customized_bb)
{
	qDebug("  Renderer_gl1::solveCurveMarkersGD");

	vector <XYZ> loc_vec_input;
     loc_vec_input.clear();

     // set pregress dialog
     PROGRESS_DIALOG( "Curve creating", widget);
     PROGRESS_PERCENT(10);

	V3dR_GLWidget* w = (V3dR_GLWidget*)widget;
	My4DImage* curImg = 0;
     if (w) curImg = v3dr_getImage4d(_idep); //by PHC, 090119

     int chno = checkCurChannel();
     if (chno<0 || chno>dim4-1)   chno = 0; //default first channel

     int nn=listCurveMarkerPool.size();
     XYZ cur_xyz;
     int i;
     for (i=0;i<nn;i++)
     {
          cur_xyz.x = listCurveMarkerPool.at(i).x-1; //marker location is 1 based
          cur_xyz.y = listCurveMarkerPool.at(i).y-1;
          cur_xyz.z = listCurveMarkerPool.at(i).z-1;
          loc_vec_input.push_back(cur_xyz);
     }

     // Prepare paras for GD, using default paras
     CurveTracePara trace_para;
	{
		trace_para.sp_num_end_nodes = 1;
		trace_para.b_deformcurve = false;
	}

     ParaShortestPath sp_para;
	sp_para.edge_select       = trace_para.sp_graph_connect; // 090621 RZC: add sp_graph_connect selection
	sp_para.background_select = trace_para.sp_graph_background; // 090829 RZC: add sp_graph_background selection
	sp_para.node_step      = trace_para.sp_graph_resolution_step; // 090610 RZC: relax the odd constraint.
	sp_para.outsample_step = trace_para.sp_downsample_step;
	sp_para.smooth_winsize = trace_para.sp_smoothing_win_sz;

	vector< vector<V_NeuronSWC_unit> > mmUnit;

     V3DLONG szx = curImg->getXDim();
     V3DLONG szy = curImg->getYDim();
     V3DLONG szz = curImg->getZDim();
     curImg->trace_z_thickness = this->thicknessZ;

     // get bounding box
     XYZ minloc, maxloc;
	if (b_customized_bb == false)
	{
          curImg->trace_bounding_box = this->dataViewProcBox; //dataBox;
          minloc.x = curImg->trace_bounding_box.x0;
          minloc.y = curImg->trace_bounding_box.y0;
          minloc.z = curImg->trace_bounding_box.z0;

          maxloc.x = curImg->trace_bounding_box.x1;
          maxloc.y = curImg->trace_bounding_box.y1;
          maxloc.z = curImg->trace_bounding_box.z1;
	}
     else
     {
          bool res = boundingboxFromStroke(minloc, maxloc);
          if(!res)
          {
               curImg->trace_bounding_box = this->dataViewProcBox; //dataBox;
               minloc.x = curImg->trace_bounding_box.x0;
               minloc.y = curImg->trace_bounding_box.y0;
               minloc.z = curImg->trace_bounding_box.z0;

               maxloc.x = curImg->trace_bounding_box.x1;
               maxloc.y = curImg->trace_bounding_box.y1;
               maxloc.z = curImg->trace_bounding_box.z1;
          }
     }

     PROGRESS_PERCENT(40);

     // loc_vec is used to store final locs on the curve
     vector <XYZ> loc_vec;
     loc_vec.clear();

     if (loc_vec_input.size()>0)
	{
		qDebug("now get curve using GD find_shortest_path_graphimg >>> ");

          loc_vec.push_back(loc_vec_input.at(0)); // the first loc is always here
          for(int ii=0; ii<loc_vec_input.size()-1; ii++)
          {
               XYZ loc0=loc_vec_input.at(ii);
               XYZ loc1=loc_vec_input.at(ii+1);

               // call GD tracing
               const char* s_error = find_shortest_path_graphimg(curImg->data4d_uint8[chno], szx, szy, szz,
                    curImg->trace_z_thickness,
                    minloc.x, minloc.y, minloc.z,
                    maxloc.x, maxloc.y, maxloc.z,
                    loc0.x, loc0.y, loc0.z,
                    1,
                    &(loc1.x), &(loc1.y), &(loc1.z),
                    mmUnit,
                    sp_para);

               if (s_error)
               {
                    throw (const char*)s_error;
                    return;
               }

               // append traced result to loc_vec
               if(!mmUnit.empty())
               {
                    // only have one seg and get the first seg
                    vector<V_NeuronSWC_unit> seg0 = mmUnit.at(0);
                    int num = seg0.size();
                    for(int j=num-2; j>=0; j-- )
                    {
                         XYZ loc;
                         loc.x=seg0.at(j).x;
                         loc.y=seg0.at(j).y;
                         loc.z=seg0.at(j).z;

                         loc_vec.push_back(loc);
                    }
               }else
               {
                    loc_vec.push_back(loc1);
               }
          }

          PROGRESS_PERCENT(80);

          loc_vec.push_back(loc_vec_input.back());

          if(loc_vec.size()<1) return; // all points are outside the volume. ZJL 110913

          // check if there is any existing neuron node is very close to the starting and ending points, if yes, then merge

          MainWindow* V3Dmainwindow = 0;
          V3Dmainwindow = v3dr_getV3Dmainwindow(_idep);

          int N = loc_vec.size();

          if (V3Dmainwindow && V3Dmainwindow->global_setting.b_3dcurve_autoconnecttips)
          {
               if (listNeuronTree.size()>0 && curEditingNeuron>0 && curEditingNeuron<=listNeuronTree.size())
               {
                    NeuronTree *p_tree = (NeuronTree *)(&(listNeuronTree.at(curEditingNeuron-1)));
                    if (p_tree)
                    {
                         V3DLONG n_id_start = findNearestNeuronNode_Loc(loc_vec.at(0), p_tree);
                         V3DLONG n_id_end = findNearestNeuronNode_Loc(loc_vec.at(N-1), p_tree);
                         qDebug("detect nearest neuron node [%ld] for curve-start and node [%ld] for curve-end for the [%d] neuron", n_id_start, n_id_end, curEditingNeuron);

                         double th_merge = 5;

                         bool b_start_merged=false, b_end_merged=false;
                         NeuronSWC cur_node;
                         if (n_id_start>=0)
                         {
                              cur_node = p_tree->listNeuron.at(n_id_start);
                              qDebug()<<cur_node.x<<" "<<cur_node.y<<" "<<cur_node.z;
                              XYZ cur_node_xyz = XYZ(cur_node.x, cur_node.y, cur_node.z);
                              if (dist_L2(cur_node_xyz, loc_vec.at(0))<th_merge)
                              {
                                   loc_vec.at(0) = cur_node_xyz;
                                   b_start_merged = true;
                                   qDebug()<<"force set the first point of this curve to the above neuron node as they are close.";
                              }
                         }
                         if (n_id_end>=0)
                         {
                              cur_node = p_tree->listNeuron.at(n_id_end);
                              qDebug()<<cur_node.x<<" "<<cur_node.y<<" "<<cur_node.z;
                              XYZ cur_node_xyz = XYZ(cur_node.x, cur_node.y, cur_node.z);
                              if (dist_L2(cur_node_xyz, loc_vec.at(N-1))<th_merge)
                              {
                                   loc_vec.at(N-1) = cur_node_xyz;
                                   b_end_merged = true;
                                   qDebug()<<"force set the last point of this curve to the above neuron node as they are close.";

                              }
                         }

                         //a special operation is that if the end point is merged, but the start point is not merged,
                         //then this segment is reversed direction to reflect the prior knowledge that a neuron normally grow out as branches
                         if (b_start_merged==false && b_end_merged==true)
                         {
                              vector <XYZ> loc_vec_tmp = loc_vec;
                              for (int i=0;i<N;i++)
                                   loc_vec.at(i) = loc_vec_tmp.at(N-1-i);
                         }
                    }
               }
          }

          PROGRESS_PERCENT(90);

          smooth_curve(loc_vec, 5);

          // adaptive curve simpling
          vector <XYZ> loc_vec_resampled;
          int stepsize = 6; // sampling stepsize
          loc_vec_resampled.clear();
          adaptiveCurveResampling(loc_vec, loc_vec_resampled, stepsize);

          if (b_addthiscurve)
          {
               addCurveSWC(loc_vec_resampled, chno);
               // used to convert loc_vec to NeuronTree and save SWC in testing
               vecToNeuronTree(testNeuronTree, loc_vec);
          }
          else //100821
          {
               b_addthiscurve = true; //in this case, always reset to default to draw curve to add to a swc instead of just  zoom
               endSelectMode();
          }
     }
     PROGRESS_PERCENT(100);
}




// get the bounding box from a stroke
bool Renderer_gl1::boundingboxFromStroke(XYZ& minloc, XYZ& maxloc)
{

     int NC = list_listCurvePos.size();
	int N = list_listCurvePos.at(0).size();

	if (NC<1 || N <3)  return false; //data is not enough and use the whole image as the bounding box

     // find min-max of x y z in loc_veci
     float minx, miny, minz, maxx, maxy, maxz;

     // Directly using stroke pos for minloc, maxloc
     for (int i=0; i<N; i++)
     {
          const MarkerPos & pos = list_listCurvePos.at(0).at(i);
          ////////////////////////////////////////////////////////////////////////
          //100730 RZC, in View space, keep for dot(clip, pos)>=0
          double clipplane[4] = { 0.0,  0.0, -1.0,  0 };
          clipplane[3] = viewClip;
          ViewPlaneToModel(pos.MV, clipplane);
          //qDebug()<<"   clipplane:"<<clipplane[0]<<clipplane[1]<<clipplane[2]<<clipplane[3];
          ////////////////////////////////////////////////////////////////////////

          XYZ loc0, loc1;
          _MarkerPos_to_NearFarPoint(pos, loc0, loc1);

          if(i==0)
          {
               minx=maxx=loc0.x; miny=maxy=loc0.y; minz=maxz=loc0.z;
          }
          if(minx>loc0.x) minx=loc0.x;
          if(miny>loc0.y) miny=loc0.y;
          if(minz>loc0.z) minz=loc0.z;

          if(maxx<loc0.x) maxx=loc0.x;
          if(maxy<loc0.y) maxy=loc0.y;
          if(maxz<loc0.z) maxz=loc0.z;

          if(minx>loc1.x) minx=loc1.x;
          if(miny>loc1.y) miny=loc1.y;
          if(minz>loc1.z) minz=loc1.z;

          if(maxx<loc1.x) maxx=loc1.x;
          if(maxy<loc1.y) maxy=loc1.y;
          if(maxz<loc1.z) maxz=loc1.z;
     }

     //
     int boundary = 10;

     minloc.x = minx - boundary;
     minloc.y = miny - boundary;
     minloc.z = minz - boundary;

     maxloc.x = maxx + boundary;
     maxloc.y = maxy + boundary;
     maxloc.z = maxz + boundary;

     dataViewProcBox.clamp(minloc);
     dataViewProcBox.clamp(maxloc);

     return true;
}


void  Renderer_gl1::vecToNeuronTree(NeuronTree &SS, vector<XYZ> loc_list)
{

	QList <NeuronSWC> listNeuron;
	QHash <int, int>  hashNeuron;
	listNeuron.clear();
	hashNeuron.clear();

     int count = 0;

     qDebug("-------------------------------------------------------");
     for (int k=0;k<loc_list.size();k++)
     {
          count++;
          NeuronSWC S;

          S.n 	= 1+k;
          S.type 	= 3;
          S.x 	= loc_list.at(k).x;
          S.y 	= loc_list.at(k).y;
          S.z 	= loc_list.at(k).z;
          S.r 	= 1;
          S.pn 	= (k==0)? -1 : k;

          //qDebug("%s  ///  %d %d (%g %g %g) %g %d", buf, S.n, S.type, S.x, S.y, S.z, S.r, S.pn);
          {
               listNeuron.append(S);
               hashNeuron.insert(S.n, listNeuron.size()-1);
          }
     }

     SS.n = -1;
     RGBA8 cc;
     cc.r=0; cc.g=20;cc.b=200;cc.a=0;
     SS.color = cc; //random_rgba8(255);//RGBA8(255, 0,0,0);
     SS.on = true;
     SS.listNeuron = listNeuron;
     SS.hashNeuron = hashNeuron;

}

/*
 * Create random positions around pos. Then get loc using mean shift method on each pos.
 * Sort the depth of all locs and get the loc at the middle of the sorted locs as the return value.
*/
void  Renderer_gl1::getMidRandomLoc(MarkerPos pos, int chno, XYZ &mid_loc)
{
     int range=6;
     srand(clock()); // initialize random seek

     vector <XYZ> rand_loc_vec;
     rand_loc_vec.clear();

     double clipplane[4] = { 0.0,  0.0, -1.0,  0 };
     clipplane[3] = viewClip;
     ViewPlaneToModel(pos.MV, clipplane);
     XYZ loc0, loc1;
     _MarkerPos_to_NearFarPoint(pos, loc0, loc1);

     XYZ loc = getCenterOfLineProfile(loc0, loc1, clipplane, chno);
     float dist = dist_L2(loc0, loc);
     rand_loc_vec.push_back(loc);

     // using map to sort dist
     map<float, int> dist_score;
     dist_score[dist] = 0;

     for(int i=0; i<2*range; i++)
     {
          // generate pos and then get new loc
          int rand_x = rand()%(range+1); // generate value from 0~range
          int rand_y = rand()%(range+1); // generate value from 0~range
          // map rand from 0~range to -range/2~range/2
          rand_x = -range/2+rand_x;
          rand_y = -range/2+rand_y;
          pos.x = pos.x + rand_x;
          pos.y = pos.y + rand_y;

          ViewPlaneToModel(pos.MV, clipplane);

          XYZ loc00, loc11;
          _MarkerPos_to_NearFarPoint(pos, loc00, loc11);

          XYZ loc_temp = getCenterOfLineProfile(loc00, loc11, clipplane, chno);
          rand_loc_vec.push_back(loc_temp);
          float dist_temp = dist_L2(loc00, loc_temp);

          dist_score[dist_temp] = rand_loc_vec.size()-1;
     }
     // sort rand_loc_vec elements in depth order and get the middle loc
     map<float, int>::iterator it;

     int count=0;
     int mid_index;
     for(it=dist_score.begin(); it!=dist_score.end(); it++)
     {
          count++;
          if(count == dist_score.size()/2)
          {
               mid_index = it->second;
               break;
          }
     }
     // get middle loc
     mid_loc = rand_loc_vec.at(mid_index);

}