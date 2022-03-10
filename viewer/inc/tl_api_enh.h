//******************************************************************************
//! \file       tl_enh_api.h
//! \brief      Interfaces Of Enhance Functions
//! \license	This source code has been released under 3-clause BSD license.
//		It includes OpenCV, OpenGL and FreeGLUT libraries. 
//		Refer to "Readme-License.txt" for details.
//******************************************************************************

#ifndef H_TL_ENH_API
#define H_TL_ENH_API

//******************************************************************************
// Include Headers
//******************************************************************************
#include <stdint.h>

#include "tl.h"


//******************************************************************************
// Definitions
//******************************************************************************
typedef struct {
	// Lens Parameters 
	int64_t			planer_prm[4];		// Planer Parameters 
	int64_t			distortion_prm[4];	// Distortion Parameters 
	// Camera Infomation 
	uint16_t		opt_axis_center_h;	// Optical Axis Center (Horizontal) [Pixel] 
	uint16_t		opt_axis_center_v;	// Optical Axis Center (Vertical) [Pixel] 
	uint16_t		sns_h;				// Number Of Sensor Output Pixels Horizontal 
	uint16_t		sns_v;				// Number Of Sensor Output Pixels Vertical 
	uint16_t		pixel_pitch;		// Hundredfold Of Pixel Pitch[Um] 
} TL_EnhInfo;


//******************************************************************************
// Functions
//******************************************************************************
TL_E_RESULT tl_enh_get_info(TL_Handle* handle, TL_EnhInfo *info);
TL_E_RESULT tl_enh_init(TL_Handle *handle, const TL_Param *param);
TL_E_RESULT tl_enh_convert_camera_coord(TL_Handle* handle, uint16_t* depth, int16_t** points);
TL_E_RESULT tl_enh_term(void);

#endif // H_TL_ENH_API 
