//******************************************************************************
//! \file       view_util_ptcd.cpp
//! \brief      Point Cloud Utilities Function.
//! \license	This source code has been released under 3-clause BSD license.
//		It includes OpenCV, OpenGL and FreeGLUT libraries. 
//		Refer to "Readme-License.txt" for details.
//******************************************************************************


//******************************************************************************
// Include Headers
//******************************************************************************
#include <stdio.h>
#include <stdlib.h>

#include <opencv2/opencv.hpp>
#include <cstring>

#include "view_util_ptcd.h"


//******************************************************************************
// Definitions
//******************************************************************************
#define SCALE_DEFAULT   (600)

typedef struct _pt_3d_t {
  float x;
  float y;
  float z;
} pt_3d_t;

typedef struct _ptcd_3d_t
{
	double ns;  //!< Timestamp In Nano Sec.
	int cnt;    //!< Data Count.
	pt_3d_t pt[MAX_PLY_SIZE];  //!< Array Of Point Cloud Data.
} ptcd_3d_t;

ptcd_3d_t     g_ply;

static float g_fov_y = 70;
static float g_z_far = 9000;
static double g_ns;  //!< Timestamp Of Point Cloud Data In Nano Sec.

int   g_wheel    = SCALE_DEFAULT/10;  //!< Scale Factor (Zoom Factor) Base On Mouse Wheel Value.
int   g_k_shift  = 0;
int   g_k_ctrl   = 0;
int   g_k_alt    = 0;
float g_offset_x = 0;
float g_offset_y = 0;
float g_offset_z = 0;
static volatile bool g_glut_running = false;  //!< Indicate glutMainLoop() Is Running Or Not.
static volatile bool g_glut_inited = 0;       //!< Indicate If OpenGL Inited Before.
static int g_gl_win_id;                       //!< OpenGL Windows ID.

static GLdouble g_eye_x = -0.2;  //!< Eye Position X.
static GLdouble g_eye_y = -0.4;  //!< Eye Position Y.
static GLdouble g_eye_z =  0.8;  //!< Eye Position Z.

static GLdouble g_tgt_x = 0.0;  //!< Target Position X.
static GLdouble g_tgt_y = 0.0;  //!< Target Position Y.
static GLdouble g_tgt_z = 0.0;  //!< Target Position Z.

static int g_cx;  //!< Drag Start Position X.
static int g_cy;  //!< Drag Start Position Y.
static int g_cx_s = 0;  //!< Drag Start Position X (Shift).
static int g_cy_s = 0;  //!< Drag Start Position Y (Shift).
static int g_cx_a = 0;  //!< Drag Start Position X (Alt).
static int g_cy_a = 0;  //!< Drag Start Position Y (Alt).

static double g_sx;  //!< Absolute Mouse Position X, Relative Position Conversion Factor In The Window.
static double g_sy;  //!< Absolute Mouse Position Y, Relative Position Conversion Factor In The Window.

static double g_cq[4] = { 1.0, 0.0, 0.0, 0.0 };  //!< Initial Value Of Rotation.
static double g_tq[4];   //!< Rotation While Dragging.
static double g_rt[16];  //!< Transformation Matrix Of Rotation.

static GLfloat g_dot_size = 1;  //!< Drawing Point Size.
static int g_depth_min  = 0;    //!< Minimum Depth Range.
static int g_refresh_ms = 30;   //!< Refresh Interval In Milliseconds.

static bool g_disp_grid      = true;  //!< Flag To Indicate Display Grid Or Not.
static bool g_disp_xyz_axis  = true;  //!< Flag To Indicate Display XYZ Axis Or Not.
static bool g_disp_guide     = true;  //!< Flag To Indicate Display Guideline Or Not.
static bool g_disp_depth_bar = true;  //!< Flag To Indicate Display Color Depth Bar Legend Or Not.
static bool g_disp_depth     = true;  //!< Flag To Indicate Display Point Cloud Or Not.
static bool g_disp_ske       = true;  //!< Flag To Indicate Display Skeleton Result Or Not.


// Rotate Matrix
static float rotate_v[16] =
{
	1.0, 0.0, 0.0, 0.0,
	0.0, 1.0, 0.0, 0.0,
	0.0, 0.0, 1.0, 0.0,
	0.0, 0.0, 0.0, 1.0,
};

// Rotate Matrix
static float rotate_v_drag[16] =
{
	1.0, 0.0, 0.0, 0.0,
	0.0, 1.0, 0.0, 0.0,
	0.0, 0.0, 1.0, 0.0,
	0.0, 0.0, 0.0, 1.0,
};

typedef struct _color_rgb_t
{
	uint8_t r;
	uint8_t g;
	uint8_t b;
} color_rgb_t;

uint8_t g_rainbow_color_tbl[3][65536];
uint32_t g_range_min = 0;
uint32_t g_range_max = 0;
uint32_t g_range = 0;


//******************************************************************************
//! \brief        Utilities Function To Build A Rainbow Color Look Up Table.
//! \n
//! \param[in]    min_val   Minimum Depth Value.
//! \param[in]    max_val   Depth Value.
//! \param[in]    range     Depth Range.
//! \param[out]   None.
//! \return       None.
//******************************************************************************
void makeColorTbl(uint32_t min_val, uint32_t max_val, uint32_t range)
{
	g_range_min = min_val;
	g_range_max = max_val;
	g_range = range;
	double rng_tmp = (double) g_range;

	//! \remark Decide Each Rainbow RGB Value Up To g_range_max.
	for (uint32_t i = 0; i < g_range_max; i++)
	{
		short ii = ((double)(i-g_range_min) / (double)(g_range_max-g_range_min))*rng_tmp > rng_tmp+512 ? g_range+512 : (short)(((double)(i-g_range_min) / (double)(g_range_max-g_range_min))*rng_tmp) + 255;

		if (ii < 0) {
			g_rainbow_color_tbl[0][i] = 255;
			g_rainbow_color_tbl[1][i] = 255;
			g_rainbow_color_tbl[2][i] = 255;
		}
		else
		if (ii < 255) {
			g_rainbow_color_tbl[0][i] = 255;
			g_rainbow_color_tbl[1][i] = 255;
			g_rainbow_color_tbl[2][i] = 255;
		}
		else
		if(ii < 511) {
			g_rainbow_color_tbl[0][i] = (char)(   255);
			g_rainbow_color_tbl[1][i] = (char)(ii-255);
			g_rainbow_color_tbl[2][i] = (char)(     0);
		}
		else
		if(ii < 766) {
			g_rainbow_color_tbl[0][i] = (char)(765-ii);
			g_rainbow_color_tbl[1][i] = (char)(   255);
			g_rainbow_color_tbl[2][i] = (char)(     0);
		}
		else
		if(ii < 1021) {
			g_rainbow_color_tbl[0][i] = (char)(     0);
			g_rainbow_color_tbl[1][i] = (char)(   255);
			g_rainbow_color_tbl[2][i] = (char)(ii-765);
		}
		else
		if(ii < 1276) {
			g_rainbow_color_tbl[0][i] = (char)(     0);
			g_rainbow_color_tbl[1][i] = (char)(1275-i);
			g_rainbow_color_tbl[2][i] = (char)(   255);
		}
		else {
			g_rainbow_color_tbl[0][i] = (char)(     0);
			g_rainbow_color_tbl[1][i] = (char)(     0);
			g_rainbow_color_tbl[2][i] = (char)(     0);
		}
	}

	//! \remark Set Pixel Below g_range_min As 255, i.e. White.
	for (uint32_t i = 0; i < g_range_min; i++) {
		g_rainbow_color_tbl[0][i] = 255;
		g_rainbow_color_tbl[1][i] = 255;
		g_rainbow_color_tbl[2][i] = 255;
	}

	//! \remark Set Pixel Above g_range_max As 0, i.e. Black.
	for (uint32_t i = g_range_max; i < 65536; i++) {
		g_rainbow_color_tbl[0][i] = 0;
		g_rainbow_color_tbl[1][i] = 0;
		g_rainbow_color_tbl[2][i] = 0;
	}
}


//******************************************************************************
//! \brief        Utilities Function To Do Quaternion Multiplication [r <- p * q].
//! \n
//! \param[in]    p[].
//! \param[in]    q[].
//! \param[out]   r[].
//! \return       None.
//******************************************************************************
void qMul(double r[], const double p[], const double q[])
{
	r[0] = p[0] * q[0] - p[1] * q[1] - p[2] * q[2] - p[3] * q[3];
	r[1] = p[0] * q[1] + p[1] * q[0] + p[2] * q[3] - p[3] * q[2];
	r[2] = p[0] * q[2] - p[1] * q[3] + p[2] * q[0] + p[3] * q[1];
	r[3] = p[0] * q[3] + p[1] * q[2] - p[2] * q[1] + p[3] * q[0];
}


//******************************************************************************
//! \brief        Utilities Function To Do Rotation Transformation Matrix [r <- Quaternion q].
//! \n
//! \param[in]    q[].
//! \param[out]   r[].
//! \return       None.
//******************************************************************************
void qRot(double r[], double q[])
{
	double x2 = q[1] * q[1] * 2.0;
	double y2 = q[2] * q[2] * 2.0;
	double z2 = q[3] * q[3] * 2.0;
	double xy = q[1] * q[2] * 2.0;
	double yz = q[2] * q[3] * 2.0;
	double zx = q[3] * q[1] * 2.0;
	double xw = q[1] * q[0] * 2.0;
	double yw = q[2] * q[0] * 2.0;
	double zw = q[3] * q[0] * 2.0;

	r[ 0] = 1.0 - y2 - z2;
	r[ 1] = xy + zw;
	r[ 2] = zx - yw;
	r[ 4] = xy - zw;
	r[ 5] = 1.0 - z2 - x2;
	r[ 6] = yz + xw;
	r[ 8] = zx + yw;
	r[ 9] = yz - xw;
	r[10] = 1.0 - x2 - y2;
	r[ 3] = r[ 7] = r[11] = r[12] = r[13] = r[14] = 0.0;
	r[15] = 1.0;
}


//******************************************************************************
//! \brief        Utilities Function To Reset All Rotation To Default.
//! \n
//! \param[in]    None.
//! \return       None.
//******************************************************************************
void resetRotate(void)
{
	g_offset_x  = 0;
	g_offset_y  = 0;
	g_offset_z  = 0;
	g_wheel     = SCALE_DEFAULT/10;
	g_depth_min = 0;

	g_eye_x = 1.5;
	g_eye_y = 0.5;
	g_eye_z = -6.0;

	g_tgt_x = 0;
	g_tgt_y = 0;
	g_tgt_z = 0;

	g_cq[0] = 1;  // x
	g_cq[1] = 0;  // y
	g_cq[2] = 0;  // z
	g_cq[3] = 0;

	memset(g_tq, 0, sizeof(g_tq)/sizeof(g_tq[0]));
	memset(g_rt, 0, sizeof(g_rt)/sizeof(g_rt[0]));

	qRot(g_rt, g_cq);

	memset(rotate_v, 0, sizeof(rotate_v)/sizeof(rotate_v[0]));
	rotate_v[0]  = 1.0;
	rotate_v[5]  = 1.0;
	rotate_v[10] = 1.0;
	rotate_v[15] = 1.0;

	memset(rotate_v_drag, 0, sizeof(rotate_v_drag)/sizeof(rotate_v_drag[0]));
	rotate_v_drag[0]  = 1.0;
	rotate_v_drag[5]  = 1.0;
	rotate_v_drag[10] = 1.0;
	rotate_v_drag[15] = 1.0;
}


//******************************************************************************
//! \brief        Utilities Function To Display Grid On Point Cloud View.
//! \n
//! \param[in]    None.
//! \return       None.
//******************************************************************************
void dispGrid(void)
{
	int grid_size = 4;

	if (g_disp_grid == false) {
		return;
	}

	//! Always Setup Properties Before glBegin().
	glLineWidth(1);
	glDisable(GL_LINE_SMOOTH);

	//! Construct And Display The Grid.
	glBegin(GL_LINES);

	glColor3ub(128, 128, 128);
	for (int i=-grid_size; i<=grid_size; ++i) {
		glVertex3f(i, 0, -grid_size);
		glVertex3f(i, 0,  grid_size);

		glVertex3f( grid_size, 0, i);
		glVertex3f(-grid_size, 0, i);
	}

	glEnd();
}


//******************************************************************************
//! \brief        Utilities Function To Display XYZ Axis On Point Cloud View.
//! \n
//! \param[in]    None.
//! \return       None.
//******************************************************************************
void dispXyzAxis(void)
{

	if (g_disp_xyz_axis == false) {
		return;
	}

	//! Always Setup Properties Before glBegin().
	glLineWidth(1);

	glBegin(GL_LINES);

	glColor3d(1, 0, 0);  //! X-Axis Red Color.
	glVertex2d(0, 0);
	glVertex2d(1, 0);

	glColor3d(0, 1, 0);  //! Y-Axis Green Color.
	glVertex2d(0,0);
	glVertex2d(0,1);

	glColor3d(0, 0, 1);  //! Z-Axis Blue Color.
	glVertex3d(0, 0, 0);
	glVertex3d(0, 0, 1);

	glEnd();

	glColor3d(1, 0, 0);  //! X-Axis Red Color Text.
	glRasterPos3d(1.1, -0.025, 0);
	glutBitmapString(GLUT_BITMAP_HELVETICA_12, (const unsigned char *)"X-axis");

	glColor3d(0, 1, 0);  //! Y-Axis Green Color Text.
	glRasterPos3d(-0.15, 1.1, 0);
	glutBitmapString(GLUT_BITMAP_HELVETICA_12, (const unsigned char *)"Y-axis");

	glColor3d(0, 0, 1);  //! Z-Axis Blue Color Text.
	glRasterPos3d(-0.15, 0, 1.1);
	glutBitmapString(GLUT_BITMAP_HELVETICA_12, (const unsigned char *)"Z-axis");
}


//******************************************************************************
//! \brief        Utilities Function To Display Guide Text On Point Cloud View.
//! \n
//! \param[in]    None.
//! \return       None.
//******************************************************************************
void dispGuideText(void)
{
	if (g_disp_guide == false) {
		return;
	}

	//! Contruct The Help Guide String.
	const char *help_str =  "F1/h = Toggle This Help Message\n"
							"F2/a = Toggle XYZ Axis Display\n"
							"F3/g = Toggle Grid Display\n"
							"F4/l = Toggle Color Depth Bar Legend Display\n"
							"F7   = Dec Depth Range          F8  = Inc Depth Range\n"
							"F9   = Dec Dot Size             F10 = Inc Dot Size\n"
							"F11/Z/WheelUp = Zoom In         F12/z/WheelDown = Zoom Out\n"
							"PageUp/w = Move Camera Forward  PageDown/r = Move Camera Backward\n"
							"Left/s   = Move Camera Left     Right/f    = Move Camera Right\n"
							"Up/e     = Move Camera Up       Down/d     = Move Camera Down\n"
							"LeftMouseHold = Rotate View\n"
							"Home/c/RightMouse = Reset View\n";

	glColor3f(1.f, 1.f, 1.f);  //! Specify White Color Text.
	glRasterPos3d(0, -0.2, 0);
	glutBitmapString(GLUT_BITMAP_8_BY_13, (const unsigned char *)help_str);  //! Display The String.
}


//******************************************************************************
//! \brief        Utilities Function To Display Depth Color Legend Bar On Point Cloud View.
//! \n
//! \param[in]    None.
//! \return       None.
//******************************************************************************
#define BOX_X -0.5
#define BOX_W  0.1
#define BOX_H  2.0
void dispDepthBar(void)
{
	GLfloat r, g, b;

	if (g_disp_depth_bar == false) {
		return;
	}

	//! Always Setup Properties Before glBegin().
	glLineWidth(1);

	//! Contruct The Depth Color Scale.
	glBegin(GL_QUAD_STRIP);

	for (int y = 0; y <= 4; y++) {
		b = (y <= 1) ? 1.f : 0.f;
		g = (1 <= y && y <= 3) ? 1.f : 0.f;
		r = (y >= 3) ? 1.f : 0.f;
		glColor3f(r, g, b);
		glVertex2f(BOX_X, (float)BOX_H / 4 * (2 - y));
		glVertex2f(BOX_X + BOX_W, (float)BOX_H / 4 * (2 - y));
	}

	glEnd();

	//! Display Legend Text.
	glColor3f(1.f, 1.f, 1.f);
	glRasterPos2d(BOX_X + BOX_W + 0.05, (float)-BOX_H / 2 );
	glutBitmapString(GLUT_BITMAP_HELVETICA_12, (const unsigned char *)"Near");
	glRasterPos2d(BOX_X + BOX_W + 0.05, (float)BOX_H / 2 );
	glutBitmapString(GLUT_BITMAP_HELVETICA_12, (const unsigned char *)"Far");
}


//******************************************************************************
//! \brief        Utilities Function To Display Depth Points On Point Cloud View.
//! \n
//! \param[in]    None.
//! \return       None.
//******************************************************************************
void dispDepthPoints(void)
{
	int i;
	GLfloat f_x;
	GLfloat f_y;
	GLfloat f_z;
	uint8_t u_x;
	uint8_t u_y;
	uint8_t u_z;
	int depth;

	g_ns = g_ply.ns;  //! Indicate Current Point Cloud's Time Stamp.

	if (g_disp_depth == false) {
		return;
	}

	//! Always Setup Properties Before glBegin().
	glPointSize(g_dot_size);     //! Setup Properties For Point Size.
	glDisable(GL_POINT_SMOOTH);  //! Setup Properties To Draw Square Points.

	//! Draw The Point Cloud Data.
	glBegin(GL_POINTS);

	for (i = 0; i < g_ply.cnt; i++) {
		f_x = (g_ply.pt[i].x);
		f_y = (g_ply.pt[i].y);
		f_z = (g_ply.pt[i].z);

		depth = (int)(f_z);  //! Convert To Integer, So That We Can Use It To Indexing.

		//! Decide The Point Color Base On Color LUT.
		u_x = g_rainbow_color_tbl[0][depth + g_depth_min];
		u_y = g_rainbow_color_tbl[1][depth + g_depth_min];
		u_z = g_rainbow_color_tbl[2][depth + g_depth_min];
		glColor3ub(u_x, u_y, u_z);

		//! Draw The Point One By One.
		f_x = f_x / g_wheel + g_offset_x;
		f_y = f_y / g_wheel + g_offset_y;
		f_z = f_z / g_wheel + g_offset_z;
		glVertex3f(f_x, f_y, f_z);
	}

	glEnd();
}


//******************************************************************************
//! \brief        Callback Handler For Idle Loop.
//! \n
//! \param[in]    None.
//! \return       None.
//! \date         2021-03-16, Tue, 02:33 PM
//******************************************************************************
void cbIdle(void)
{
	glutPostRedisplay();
}


//******************************************************************************
//! \brief        Callback Handler For Mouse Button Click Event. Trigger When Mouse Button Click.
//! \n
//! \param[in]    button    Mouse Button Number.
//! \param[in]    x         X Coordinate.
//! \param[in]    y         Y Coordinate.
//! \param[out]   None.
//! \return       None.
//******************************************************************************
void cbMouse(int button, int state, int x, int y)
{
	int k_state = glutGetModifiers();

	g_k_shift = (k_state & GLUT_ACTIVE_SHIFT) ? 1 : 0;  //! SHIFT Key Pressed.
	g_k_ctrl  = (k_state & GLUT_ACTIVE_CTRL)  ? 1 : 0;  //! CTRL Key Pressed.
	g_k_alt   = (k_state & GLUT_ACTIVE_ALT)   ? 1 : 0;  //! ALT Key Pressed.

	if ((g_k_shift == 1) || (g_k_ctrl == 1) || (g_k_alt == 1)) {
		switch (button) {
			case GLUT_LEFT_BUTTON:
				switch (state) {
					case GLUT_DOWN:  //! Record The Drag Start Point.
						g_cx_s = x;
						g_cy_s = y;
						g_cx_a = x;
						g_cy_a = y;
						glutIdleFunc(cbIdle);  //! Animation Start.
						break;

					case GLUT_UP:
						glutIdleFunc(0);  //! Animation End.
						break;

					default:
						break;
				}
				break;

			//case GLUT_MIDDLE_BUTTON:
			case GLUT_RIGHT_BUTTON:
				resetRotate();
				break;

			default:
				break;
		}
	}
	else
	if (g_k_shift == 0) {
		switch (button) {
			case GLUT_LEFT_BUTTON:
				switch (state) {
					case GLUT_DOWN:  //! Record The Drag Start Point.
						g_cx = x;
						g_cy = y;
						g_cx_s = x;
						g_cy_s = y;
						g_cx_a = x;
						g_cy_a = y;
						glutIdleFunc(cbIdle);  //! Animation Start.
						break;

					case GLUT_UP:
						glutIdleFunc(0);  //! Animation End.

						g_cq[0] = g_tq[0];  //! Save Rotation.
						g_cq[1] = g_tq[1];
						g_cq[2] = g_tq[2];
						g_cq[3] = g_tq[3];
						break;

					default:
						break;
				}
				break;

			//case GLUT_MIDDLE_BUTTON:
			case GLUT_RIGHT_BUTTON:
				resetRotate();
				break;

			// https://stackoverflow.com/questions/14378/using-the-mouse-scrollwheel-in-glut
			// The solution is to use the regular glutMouseFunc callback and check for button == 3 for wheel up, and button == 4 for wheel down
			case 3:  //! Wheel Up
			case 4:  //! Wheel Down
			{
				int isZoomIn = (button == 3) ? -1 : 1;
				g_wheel += (15 * isZoomIn);
				if (g_wheel <= 15) {
					g_wheel = 15;
				}
			}
				break;

			default:
				break;
		}
	}
}


//******************************************************************************
//! \brief        Callback Handler For Mouse Wheel Scroll Event. Trigger When Mouse Wheel Scroll.
//! \n
//! \param[in]    wheel_number  Mouse Wheel Number.
//! \param[in]    direction     Scroll Direction.
//! \param[in]    x             X Coordinate.
//! \param[in]    y             Y Coordinate.
//! \param[out]   None.
//! \return       None.
//******************************************************************************
// https://stackoverflow.com/questions/14378/using-the-mouse-scrollwheel-in-glut
// The solution is to use the regular glutMouseFunc callback and check for button == 3 for wheel up, and button == 4 for wheel down
void cbMouseWheel(int wheel_number, int direction, int x, int y)
{
	wheel_number = wheel_number;  // Resolve Build Warning.
	x = x;  // Resolve Build Warning.
	y = y;  // Resolve Build Warning.

	g_wheel += (15 * direction);
	if (g_wheel <= 15) {
		g_wheel = 15;
	}
}


//******************************************************************************
//! \brief        Callback Handler For Mouse Movement Event. Trigger When Mouse Move.
//! \n
//! \param[in]    x X Coordinate.
//! \param[in]    y Y Coordinate.
//! \param[out]   None.
//! \return       None.
//******************************************************************************
#define SCALE           (2.0 * 3.14159265358979323846)
void cbMotion(int x, int y)
{
	double dx, dy, a;

	int k_state = glutGetModifiers();

	g_k_shift = (k_state & GLUT_ACTIVE_SHIFT) ? 1 : 0;  //! SHIFT Key Pressed.
	g_k_ctrl  = (k_state & GLUT_ACTIVE_CTRL)  ? 1 : 0;  //! CTRL Key Pressed.
	g_k_alt   = (k_state & GLUT_ACTIVE_ALT)   ? 1 : 0;  //! ALT Key Pressed.

	if (g_k_ctrl == 1) {
		//! Displacement Of Mouse Pointer Position From Drag Start Position.
		g_offset_x += (x - g_cx_s) * g_sx * 6 * ((float)SCALE_DEFAULT / g_wheel);
		g_offset_y += (g_cy_s - y) * g_sy * 6 * ((float)SCALE_DEFAULT / g_wheel);

		g_cx_s = x;
		g_cy_s = y;
	}
	else
	if (g_k_shift == 1) {
		//! Displacement Of Mouse Pointer Position From Drag Start Position.
		g_eye_z -= (g_cy_s - y) * g_sy * 6 * ((float)SCALE_DEFAULT / g_wheel);

		if (g_eye_z < 0) {
			g_tgt_z = g_eye_z - 1;
		}
		else {
			g_tgt_z = 0;
		}

		g_cx_s = x;
		g_cy_s = y;
	}
	else
	if (g_k_alt == 1) {
		//! Displacement Of Mouse Pointer Position From Drag Start Position.
		g_offset_z += (g_cy_a - y) * g_sy * 6 * ((float)SCALE_DEFAULT / g_wheel);

		g_cx_a = x;
		g_cy_a = y;
	}
	else
	if (g_k_shift == 0) {
		//! Displacement Of Mouse Pointer Position From Drag Start Position.
		dx = (x - g_cx) * g_sx;
		dy = (y - g_cy) * g_sy;

		//! Distance From Mouse Pointer Position To Drag Start Position.
		a = sqrt(dx * dx + dy * dy);

		if (a != 0.0) {
			//! Find The Quaternion dq Of Rotation With Mouse Drag
			//double ar = a * SCALE * 0.125;
			double ar = a * SCALE * 0.0625;
			double as = sin(ar) / a;
			double dq[4] = { cos(ar), dy * as, dx * as, 0.0 };

			//! Combine Rotation By Multiplying The Initial Value g_cq Of Rotation By dq
			qMul(g_tq, dq, g_cq);

			//! Find The Transformation Matrix Of Rotation From A Quaternion
			qRot(g_rt, g_tq);
		}
	}
}


//******************************************************************************
//! \brief        Callback Handler For Special Key Press Event. Trigger When Special Key Is Pressed.
//! \n
//! \param[in]    key   New Key Code.
//! \param[in]    x     X Coordinate.
//! \param[in]    y     Y Coordinate.
//! \param[out]   None.
//! \return       None.
//******************************************************************************
void cbSpecialKey(int key, int x, int y)
{
	x = x;  // Resolve Build Warning.
	y = y;  // Resolve Build Warning.

	switch (key) {
		case GLUT_KEY_F1:  //! F1 Key : Toggle Help Text Display On Point Cloud Window.
			g_disp_guide = !g_disp_guide;
			break;

		case GLUT_KEY_F2:  //! F2 Key : Toggle Axis Display On Point Cloud Window. (RGB = XYZ)
			g_disp_xyz_axis = !g_disp_xyz_axis;
			break;

		case GLUT_KEY_F3:  //! F3 Key : Toggle Grid Display On Point Cloud Window.
			g_disp_grid = !g_disp_grid;
			break;

		case GLUT_KEY_F4:  //! F4 Key : Toggle Color Depth Bar Legend Display On Point Cloud Window.
			g_disp_depth_bar = !g_disp_depth_bar;
			break;

		case GLUT_KEY_F7:  //! F7 Key : Dec Depth Range.
			g_depth_min += 50;
			break;

		case GLUT_KEY_F8:  //! F8 Key : Inc Depth Range.
			if (g_depth_min >= 50) {
				g_depth_min -= 50;
			}
			break;

		case GLUT_KEY_F9:  //! F9 Key : Dec Dot Size.
			if (g_dot_size > 0) {
				g_dot_size -= 1;
			}
			break;

		case GLUT_KEY_F10:  //! F10 : Inc Dot Size.
			if (g_dot_size < 100) {
				g_dot_size += 1;
			}
			break;

		case GLUT_KEY_F12:  //! F12 : Zoom In.
		case GLUT_KEY_F11:  //! F11 : Zoom Out.
		{
			int isZoomIn = (key == GLUT_KEY_F11) ? -1 : 1;
			g_wheel += (15 * isZoomIn);
			if (g_wheel <= 15) {
				g_wheel = 15;
			}
		}
			break;

		case GLUT_KEY_PAGE_UP:  //! Page Up Key : Move Camera Forward.
			g_eye_z -= 0.1;
			break;

		case GLUT_KEY_PAGE_DOWN:  //! Page Down Key : Move Camera Backward.
			g_eye_z += 0.1;
			break;

		case GLUT_KEY_LEFT:  //! Left Key : Move Camera Left.
			g_eye_x -= 0.1;
			break;

		case GLUT_KEY_RIGHT:  //! Right Key : Move Camera Right.
			g_eye_x += 0.1;
			break;

		case GLUT_KEY_UP:  //! Up Key : Move Camera Up.
			g_eye_y += 0.1;
			break;

		case GLUT_KEY_DOWN:  //! Down Key : Move Camera Down.
			g_eye_y -= 0.1;
			break;

		case GLUT_KEY_HOME:  //! Home Key : Reset View Setting.
			resetRotate();
			break;

		default:
			break;
	}
}


//******************************************************************************
//! \brief        Callback Handler For Key Press Event. Trigger When Keybaord Key Pressed.
//! \n
//! \param[in]    key   New Key Code.
//! \param[in]    x     X Coordinate.
//! \param[in]    y     Y Coordinate.
//! \param[out]   None.
//! \return       None.
//******************************************************************************
void cbKeyboard(unsigned char key, int x, int y)
{
	x = x;  // Resolve Build Warning.
	y = y;  // Resolve Build Warning.

	switch (key) {
		case 'h':  //! h : Toggle Help Text Display On Point Cloud Window.
			g_disp_guide = !g_disp_guide;
			break;

		case 'a':  //! g : Toggle Axis Display On Point Cloud Window. (RGB = XYZ)
			g_disp_xyz_axis = !g_disp_xyz_axis;
			break;

		case 'g':  //! g : Toggle Grid Display On Point Cloud Window.
			g_disp_grid = !g_disp_grid;
			break;

		case 'l':  //! l : Toggle Color Depth Bar Legend Display On Point Cloud Window.
			g_disp_depth_bar = !g_disp_depth_bar;
			break;

		case 'p':  //! p : Toggle Color Depth Display On Point Cloud Window.
			g_disp_depth = !g_disp_depth;
			break;

		case 'w':  //! e : Move Camera Forward.
			g_eye_z -= 0.1;
			break;

		case 'r':  //! d : Move Camera Backward.
			g_eye_z += 0.1;
			break;

		case 's':  //! s : Move Camera Left.
			g_eye_x -= 0.1;
			break;

		case 'f':  //! f : Move Camera Right.
			g_eye_x += 0.1;
			break;

		case 'e':  //! t : Move Camera Up.
			g_eye_y += 0.1;
			break;

		case 'd':  //! g : Move Camera Down.
			g_eye_y -= 0.1;
			break;

		case 'c':  //! c : Reset View Setting.
			resetRotate();
			break;

		case 'Z':  //! Z : Zoom In.
		case 'z':  //! z : Zoom Out.
		{
			int isZoomIn = (key == 'Z') ? -1 : 1;
			g_wheel += (15 * isZoomIn);
			if (g_wheel <= 15) {
				g_wheel = 15;
			}
		}
			break;

		default:
			break;
	}
}


//******************************************************************************
//! \brief        Callback Handler For Window Re-Size Event. Trigger When The Window First Appears And Whenever The Window Is Re-Sized With Its New Width And Height.
//! \n
//! \param[in]    width     New Width.
//! \param[in]    height    New Height.
//! \param[out]   None.
//! \return       None.
//******************************************************************************
void cbReshape(GLsizei width, GLsizei height)
{
	//! For Converting The Mouse Pointer Position To A Relative Position In The Window
	g_sx = 1.0 / (double) width;
	g_sy = 1.0 / (double) height;

	//! Make The Entire Window A Viewport
	glViewport(0, 0, width, height);

	///! Specifying The Perspective Transformation Matrix
	glMatrixMode(GL_PROJECTION);

	//! Initialization Of Perspective Transformation Matrix
	glLoadIdentity();
	gluPerspective(g_fov_y, (double)width / (double) height, 0.0, g_z_far);

	//! Specifying The Model View Transformation Matrix
	glMatrixMode(GL_MODELVIEW);
}


//******************************************************************************
//! \brief        Callback Handler For Window-Repaint Event.
//! \n
//! \param[in]    None.
//! \param[out]   None.
//! \return       None.
//******************************************************************************
void cbDisplay(void)
{
	//! \remark Check If glut Inited Before.
	if (g_glut_inited == false) {
		//! \remark Check If glutMainLoop() Is Runing, If Not, Destroy Windows And Abort.
		if (g_glut_running == false) {
			//! Destroy View Display.
			if (g_gl_win_id >= 0) {
				glutSetWindow(g_gl_win_id);
				glutDestroyWindow(g_gl_win_id);
				g_gl_win_id = -1;
			}
			return;
		}
		return;
	}

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); //! Clear Color And Depth Buffers
	//glMatrixMode(GL_MODELVIEW);  //! To Operate On Model-View Matrix
	glLoadIdentity();   //! Reset The Model-View Matrix

	g_tgt_x = g_eye_x;  //! Update Viewpoint X.
	g_tgt_y = g_eye_y;  //! Update Viewpoint Y.

	gluLookAt(g_eye_x, g_eye_y, g_eye_z, g_tgt_x, g_tgt_y, g_tgt_z, 0.0, 1.0, 0.0);
	glMultMatrixd(g_rt);

	//! ----------------------------------------------------------------------
	//! Draw Grid
	dispGrid();

	//! ----------------------------------------------------------------------
	//! Draw XYZ Axis
	dispXyzAxis();

	//! ----------------------------------------------------------------------
	//! Draw Help Text
	dispGuideText();

	//! ----------------------------------------------------------------------
	//! Draw Depth Bar
	dispDepthBar();

	//! ----------------------------------------------------------------------
	//! Draw Depth Point Cloud
	dispDepthPoints();

	glFlush();
	glutSwapBuffers();  //! Swap The Front And Back Frame Buffers (Double Buffering)
}


//******************************************************************************
//! \brief        Callback Handler For Window-Repaint Event. Trigger When Timer Expired.
//! \n
//! \param[in]    value Tiemr Value.
//! \param[out]   None.
//! \return       None.
//******************************************************************************
void cbRefreshTimer(int value) {
	value = value;  // Resolve Build Warning.

	glutPostRedisplay();  //! Post Re-Paint Request To Trigger cbDisplay()

	if (g_glut_running == true) {
		glutTimerFunc(g_refresh_ms, cbRefreshTimer, 0);  //! Register Again Next Timer Call x Milliseconds Later.
	}
}


//******************************************************************************
//! \brief        Update Point Cloud Data Into Global Array "g_ply".
//! \n
//! \param[in]    ts_ns     Time Stamps In Nano Sec.
//! \param[in]    ply_dat   Pointer To Point Cloud Data.
//! \param[in]    ply_cnt   Point Cloud Data Count.
//! \param[out]   None.
//! \return       None.
//******************************************************************************
void update3dData(double ts_ns, int16_t *ply_dat, int32_t ply_cnt)
{
	int32_t i;
	int16_t *ptr_dat = ply_dat;
	int16_t x;
	int16_t y;
	int16_t z;

	//! \remark Check If glut Inited Before, If Not, Abort.
	if (g_glut_inited == false) {
		return;
	}

	//! \remark Check If glutMainLoop() Is Runing, If Not, Abort.
	if (g_glut_running == false) {
		return;
	}

	g_ply.ns = ts_ns;  //! Save The Time Stamps.
	g_ply.cnt = ply_cnt;  //! Save The Data Count.

	//! \remark Iterate Through All The Point Cloud Data.
	for (i = 0; i < g_ply.cnt; i++) {
		x = *ptr_dat++;
		y = *ptr_dat++;
		z = *ptr_dat++;

		g_ply.pt[i].x = (float) x;
		g_ply.pt[i].y = (float) y;
		if ( (float) z > (float) g_depth_min ) {
			g_ply.pt[i].z = (float)(z - g_depth_min);
		}
		else {
			g_ply.pt[i].z = (float)(0xFFFF);
		}
	}
}


//******************************************************************************
//! \brief        Point Cloud View Main Function To Trigger glutMainLoop().
//! \n
//! \param[in]    fov_y     Field Of View.
//! \param[in]    z_far     Depth Limit.
//! \param[out]   None.
//! \return       None.
//******************************************************************************
int mainPtCloudView(float fov_y, float z_far, const char *title)
{
	int  argc = 0;
	char argv[1][1];

	g_fov_y = fov_y;
	g_z_far = z_far;

	glutInit(&argc, (char **)argv);
	g_glut_inited = true;

	glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);  //! Enable Rgb Mode, Double Buffered Mode, Depth Mode.
	glutInitWindowSize(640, 480);       //! Set The Window's Initial Width & Height.
	glutInitWindowPosition(640, 240);   //! Position The Window.
	g_gl_win_id = glutCreateWindow(title);

	//glutInitWindowSize(glutGet(GLUT_SCREEN_WIDTH) - 80, glutGet(GLUT_SCREEN_HEIGHT) - 60);
	glutSetOption(GLUT_ACTION_ON_WINDOW_CLOSE, GLUT_ACTION_GLUTMAINLOOP_RETURNS);  //! Set Control To Return To The Function Which Called glutMainLoop();

	glutDisplayFunc(cbDisplay); //! Register Display Callback Function.
	glutReshapeFunc(cbReshape); //! Register Resize Callback Function.

	glutMouseFunc(cbMouse);             //! Register Mouse Callback Function.
	glutMotionFunc(cbMotion);           //! Register Mouse Motion Callback Function.
	glutMouseWheelFunc(cbMouseWheel);   //! Register Mouse Wheel Callback Function.
	glutKeyboardFunc(cbKeyboard);       //! Register Key Press Callback Function.
	glutSpecialFunc(cbSpecialKey);      //! Register Special Key Press Callback Function.

	//glClearColor(1.0, 1.0, 1.0, 1.0);     //! Background Color Change (White)
	//glClearColor(0.0, 0.0, 0.0, 0.0);     //! Background Color Change (Black)
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);   //! Set Background Color To Black And Opaque

	glClearDepth(1.0f);         //! Set Background Depth To Farthest
	glEnable(GL_DEPTH_TEST);    //! Enable Depth Testing For Z-Culling
	glDepthFunc(GL_LEQUAL);     //! Set The Type Of Depth-Test
	glShadeModel(GL_SMOOTH);    //! Enable Smooth Shading
	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);  //! Nice Perspective Corrections
	//glOrtho(-1, 1, -1, 1, 2, 4);

	//! Initialization Rotation Setting.
	resetRotate();

	//! Initialization Of Rotation Matrix.
	qRot(g_rt, g_cq);

	//! Trigger First Timer Call Immediately
	glutTimerFunc(0, cbRefreshTimer, 0);

	//! Indicate Enter glutMainLoop().
	g_glut_running = true;

	//! Run glutMainLoop().
	glutMainLoop();

	return 0;
}


//******************************************************************************
//! \brief        Point Cloud View Main Function To Trigger glutLeaveMainLoop().
//! \n
//! \param[in]    None.
//! \param[out]   None.
//! \return       None.
//******************************************************************************
void mainPtCloudViewExit(void)
{
	if ( (g_glut_inited == true) || (g_glut_running == true) ) {
		//! Indicate Glut Not Yet Init.
		g_glut_inited = false;

		//! Indicate Exit glutMainLoop().
		g_glut_running = false;

		//! Exit glutMainLoop(), i.e. Enter glutLeaveMainLoop().
		glutLeaveMainLoop();
	}
}
