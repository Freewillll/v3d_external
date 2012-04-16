
/*
 * Copyright (c)2006-2010  Hanchuan Peng (Janelia Farm, Howard Hughes Medical Institute).
 * All rights reserved.
 */


/************
                                            ********* LICENSE NOTICE ************

This folder contains all source codes for the V3D project, which is subject to the following conditions if you want to use it.

You will ***have to agree*** the following terms, *before* downloading/using/running/editing/changing any portion of codes in this package.

1. This package is free for non-profit research, but needs a special license for any commercial purpose. Please contact Hanchuan Peng for details.

2. You agree to appropriately cite this work in your related studies and publications.

Peng, H., Ruan, Z., Long, F., Simpson, J.H., and Myers, E.W. (2010) “V3D enables real-time 3D visualization and quantitative analysis of large-scale biological 
image data sets,” Nature Biotechnology, Vol. 28, No. 4, pp. 348-353, DOI: 10.1038/nbt.1612. ( http://penglab.janelia.org/papersall/docpdf/2010_NBT_V3D.pdf )

Peng, H, Ruan, Z., Atasoy, D., and Sternson, S. (2010) “Automatic reconstruction of 3D neuron structures using a graph-augmented deformable model,” Bioinformati
cs, Vol. 26, pp. i38-i46, 2010. ( http://penglab.janelia.org/papersall/docpdf/2010_Bioinfo_GD_ISMB2010.pdf )

3. This software is provided by the copyright holders (Hanchuan Peng), Howard Hughes Medical Institute, Janelia Farm Research Campus, and contributors "as is" a
nd any express or implied warranties, including, but not limited to, any implied warranties of merchantability, non-infringement, or fitness for a particular pu
rpose are disclaimed. In no event shall the copyright owner, Howard Hughes Medical Institute, Janelia Farm Research Campus, or contributors be liable for any di
rect, indirect, incidental, special, exemplary, or consequential damages (including, but not limited to, procurement of substitute goods or services; loss of us
e, data, or profits; reasonable royalties; or business interruption) however caused and on any theory of liability, whether in contract, strict liability, or to
rt (including negligence or otherwise) arising in any way out of the use of this software, even if advised of the possibility of such damage.

4. Neither the name of the Howard Hughes Medical Institute, Janelia Farm Research Campus, nor Hanchuan Peng, may be used to endorse or promote products derived 
from this software without specific prior written permission.

*************/


//by Hanchuan Peng, 2012-4-6

#ifndef __VAA3D_NEURONTOOLBOX_PARA_H__
#define __VAA3D_NEURONTOOLBOX_PARA_H__

#include "../v3d_basicdatatype.h"
#include "../basic_4dimage.h"
#include "../basic_surf_objs.h"
#include "../v3d_interface.h"

struct vaa3d_neurontoolbox_paras
{
	QString OPS; //operation

	NeuronTree nt;//the neuron selected
	V3dR_MainWindow * win;//current 3D Viewer
	V3DLONG n_id;

//	Image4DSimple *imgp; //the image data for a plugin to call	
//	V3DLONG xs, ys, zs; //starting coordinates (in pixel space)
//	V3DLONG xe, ye, ze; //ending coordinates (in pixel space)
//	double xrez, yrez, zrez; //imaging step size in 3D (in physical space, and also assuming the unit (e.g. micron) is unchanging)

//	LandmarkList list_landmarks; //a series of 3D locations for ablation
};

#endif

