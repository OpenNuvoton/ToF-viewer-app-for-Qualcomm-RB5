//******************************************************************************
//! \file       view_util_ptcd.h
//! \brief      Point Cloud Utilities Function Header File.
//! \license	This source code has been released under 3-clause BSD license.
//		It includes OpenCV, OpenGL and FreeGLUT libraries. 
//		Refer to "Readme-License.txt" for details.
//******************************************************************************

#ifndef _VIEW_UTIL_PTCD_H_
#define _VIEW_UTIL_PTCD_H_


//******************************************************************************
// Include Headers
//******************************************************************************
#include <GL/freeglut.h>
#include <math.h>


//******************************************************************************
// Definitions
//******************************************************************************
#define MAX_PLY_SIZE    (640 * 480 * 2)


//******************************************************************************
// Functions
//******************************************************************************
void makeColorTbl(uint32_t min_val, uint32_t max_val, uint32_t range);
void update3dData(double ts_ns, int16_t *ply_dat, int32_t ply_cnt);
int mainPtCloudView(float fov_y, float z_far, const char *title);
void mainPtCloudViewExit(void);


#endif  // _VIEW_UTIL_PTCD_H_
