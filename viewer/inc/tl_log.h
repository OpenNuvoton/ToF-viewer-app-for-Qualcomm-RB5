//******************************************************************************
//! \file       tl_log.h
//! \brief      Log Functions
//! \license	This source code has been released under 3-clause BSD license.
//		It includes OpenCV, OpenGL and FreeGLUT libraries. 
//		Refer to "Readme-License.txt" for details.
//******************************************************************************

#ifndef H_TL_LOG
#define H_TL_LOG


#include <stdio.h>

/* Information log */
#define TL_LGI(fmt, ...)
//#define TL_LGI(fmt, ...)	(fprintf(stderr, "[%s:%d] \n" fmt, __func__, __LINE__, ## __VA_ARGS__))

/* Error log */
//#define TL_LGE(fmt, ...)
#define TL_LGE(fmt, ...)	(fprintf(stderr, "[%s:%d] \n" fmt, __func__, __LINE__, ## __VA_ARGS__))


#endif	/* H_TL_LOG */
