//******************************************************************************
//! \file       viewer.cpp
//! \brief      Sample viewer application of tof library.
//! \license	This source code has been released under 3-clause BSD license.
//		It includes OpenCV, OpenGL and FreeGLUT libraries. 
//		Refer to "Readme-License.txt" for details.
//******************************************************************************

//******************************************************************************
// Include Headers
//******************************************************************************
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <signal.h>

#include <cstring>
#include <chrono>

#include <opencv2/opencv.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#include "tl.h"
#include "tl_log.h"
#include "tl_api_enh.h"
#include "view_util_ptcd.h"


//******************************************************************************
// Definitions
//******************************************************************************
#define VIEWER_VERSION						(0x0001)

#define OPENCV_WINDOW_NAME_DPTH				"Color Depth Image"
#define OPENCV_WINDOW_NAME_IR				"IR image"
#define OPENCV_WINDOW_NAME_BG				"BG image"
#define OPENCV_TRACKBAR_NAME_GAMMA_CORR_IR	"IR Gamma Correction (slider/10)"
#define OPENCV_TRACKBAR_NAME_GAMMA_CORR_BG	"BG Gamma Correction (slider/10)"

#define OPENGL_WINDOW_NAME_PTCD				"Point Cloud View"

// Image Size
typedef struct {
	size_t	depth;	// depth image
	size_t	ir;		// ir image
	size_t	bg;		// bg image
} apl_img_size;

// Application Paramters
typedef struct {
	TL_Handle			*handle;		// camera handle
	TL_ModeInfoGroup	mode_info_grp;	// Each ranging mode info
	TL_DeviceInfo		device_info;	// Device info
	TL_Fov				fov;			// FOV info
	TL_LensPrm			lens_info;		// Lens info
	TL_E_MODE			mode;			// User's selected ranging mode
	TL_E_IMAGE_KIND		image_kind;		// User's selected image kind, type of output image
	TL_Resolution		resolution;		// resolution of images
	apl_img_size		img_size;		// image size
	int16_t				*points_cloud;	// data pointer of PointCloud
} apl_prm;

static apl_prm gPrm;					// application parameters
volatile bool bExit = false;			// false = Run Program, true = Exit Program.

pthread_t threadview;					// View Thread (Handle Depth View, IR View)
pthread_t threadview3d;					// 3D View Thread (Handle 3D Point Cloud View)


//******************************************************************************
// Functions
//******************************************************************************
void apl_print_error(TL_E_RESULT ret, char *function, unsigned int line);
void apl_callback(TL_Handle *handle, uint32_t notify, TL_Image data);
void apl_show_img(TL_E_MODE mode, TL_E_IMAGE_KIND img_kind, TL_Resolution reso, TL_Image *stData);
void *menu_thread(void *);
void *view_thread(void *);
void *view_3d_thread(void *);


//******************************************************************************
//! \brief        Utilities Function To Init Depth Image To Color Map, By Using Look Up Table.
//! \n
//! \param[in]    min_val   Minimum Depth Value.
//! \param[in]    max_val   Maximum Depth Value.
//! \param[in]    range     Depth Range.
//! \param[out]   None.
//! \return       None.
//******************************************************************************
void apl_init_color_tbl(uint32_t min_val, uint32_t max_val, uint32_t range)
{
  makeColorTbl(min_val, max_val, range);
}


//******************************************************************************
//! \brief        Initialization of libccdtof.so Library
//! \details
//! \param[in]    None
//! \param[out]   None
//! \return       0         success
//! \return       -1        failed
//******************************************************************************
static int apl_init(TL_E_MODE mode, TL_E_IMAGE_KIND image_kind)
{
	TL_E_RESULT ret;
	TL_Param    tlprm;

	memset(&tlprm, 0, sizeof(tlprm));

	// Default Image Kind
	gPrm.image_kind = image_kind;
	tlprm.image_kind = image_kind;

	// Default Mode
	gPrm.mode = mode;

	// Initialize libccdtof.so Library
	gPrm.handle = NULL;
	ret = TL_init(&gPrm.handle, &tlprm);
	if (ret != TL_E_SUCCESS) {
		apl_print_error(ret, (char *)"TL_init", __LINE__);
		return -1;
	}

	// Set Depth Mode (Ranging Mode)
	ret = TL_setProperty(gPrm.handle, TL_CMD_MODE, (void*)&gPrm.mode);
	if (ret != TL_E_SUCCESS) {
		apl_print_error(ret, (char *)"TL_setProperty TL_CMD_MODE", __LINE__);
		return -1;
	}

	// Get Resolution Of Output Images
	ret = TL_getProperty(gPrm.handle, TL_CMD_RESOLUTION, (void*)&gPrm.resolution);
	if (ret != TL_E_SUCCESS) {
		apl_print_error(ret, (char *)"TL_getProperty TL_CMD_RESOLUTION", __LINE__);
		return -1;
	}
	else {
		printf("Resolution of output images:\n");
		printf("depth : width=%d, height=%d, stride=%d, bit_per_pixel=%d \n",
			gPrm.resolution.depth.width,
			gPrm.resolution.depth.height,
			gPrm.resolution.depth.stride,
			gPrm.resolution.depth.bit_per_pixel);
		printf("ir    : width=%d, height=%d, stride=%d, bit_per_pixel=%d \n",
			gPrm.resolution.ir.width,
			gPrm.resolution.ir.height,
			gPrm.resolution.ir.stride,
			gPrm.resolution.ir.bit_per_pixel);
		printf("bg    : width=%d, height=%d, stride=%d, bit_per_pixel=%d \n",
			gPrm.resolution.bg.width,
			gPrm.resolution.bg.height,
			gPrm.resolution.bg.stride,
			gPrm.resolution.bg.bit_per_pixel);
		printf("\n");
	}

	// Get Mode Information
	ret = TL_getProperty(gPrm.handle, TL_CMD_MODE_INFO, (void*)&gPrm.mode_info_grp);
	if (ret != TL_E_SUCCESS) {
		apl_print_error(ret, (char *)"TL_getProperty TL_CMD_MODE_INFO", __LINE__);
		return -1;
	}
	else {
		printf("Mode Info:\n");
		printf("mode0 : enable=%d, range_near=%d, range_far=%d, depth_unit=%d, fps=%d\n",
				gPrm.mode_info_grp.mode[0].enable,
				gPrm.mode_info_grp.mode[0].range_near,
				gPrm.mode_info_grp.mode[0].range_far,
				gPrm.mode_info_grp.mode[0].depth_unit,
				gPrm.mode_info_grp.mode[0].fps);
		printf("mode1 : enable=%d, range_near=%d, range_far=%d, depth_unit=%d, fps=%d\n",
				gPrm.mode_info_grp.mode[1].enable,
				gPrm.mode_info_grp.mode[1].range_near,
				gPrm.mode_info_grp.mode[1].range_far,
				gPrm.mode_info_grp.mode[1].depth_unit,
				gPrm.mode_info_grp.mode[1].fps);
		printf("\n");
	}

	// Get Fov Information
	ret = TL_getProperty(gPrm.handle, TL_CMD_FOV, (void*)&gPrm.fov);
	if(ret != TL_E_SUCCESS){
		apl_print_error(ret, (char *)"TL_getProperty TL_CMD_FOV", __LINE__);
		return -1;
	}
	else {
		printf("Fov Info:\n");
		printf("focal_length=%d, angle_h=%d, angle_v=%d\n",
				gPrm.fov.focal_length,
				gPrm.fov.angle_h,
				gPrm.fov.angle_v);
		printf("\n");
	}

	// Get Mode Information
	ret = TL_getProperty(gPrm.handle, TL_CMD_DEVICE_INFO, (void*)&gPrm.device_info);
	if (ret != TL_E_SUCCESS) {
		apl_print_error(ret, (char *)"TL_getProperty TL_CMD_DEVICE_INFO", __LINE__);
		return -1;
	}
	else {
		printf("Hardware Info:\n");
		printf("%s %s %s %s \n",
			gPrm.device_info.mod_name,
			gPrm.device_info.afe_name,
			gPrm.device_info.sns_name,
			gPrm.device_info.lns_name);
		printf("mod_type:0x%x 0x%x afe_ptn_id:0x%x sno_l:0x%x\n",
			gPrm.device_info.mod_type1,
			gPrm.device_info.mod_type2,
			gPrm.device_info.afe_ptn_id,
			gPrm.device_info.sno_l);
		printf("map_ver:0x%x sno_u:0x%x ajust_date:0x%x ajust_no:0x%x\n",
			gPrm.device_info.map_ver,
			gPrm.device_info.sno_u,
			gPrm.device_info.ajust_date,
			gPrm.device_info.ajust_no);
		printf("\n");
	}

	//! \remark - Decide The Range For Depth Base On Range Mode.
	apl_init_color_tbl(gPrm.mode_info_grp.mode[mode].range_near, gPrm.mode_info_grp.mode[mode].range_far, 1000);

	// Get device information, Execute TL_getProperty (TL_CMD_LENS_INFO)
	ret = TL_getProperty(gPrm.handle, TL_CMD_LENS_INFO, (void*)&gPrm.lens_info);
	if (ret != TL_E_SUCCESS) {
		apl_print_error(ret, (char *)"TL_getProperty TL_CMD_LENS_INFO", __LINE__);
		return -1;
	}
	else {
		printf("Lens Info:\n");
		printf("sns_h=%d, sns_v=%d, center_h=%d, center_v=%d pixel_pitch=%d\n",
			gPrm.lens_info.sns_h,
			gPrm.lens_info.sns_v,
			gPrm.lens_info.center_h,
			gPrm.lens_info.center_v,
			gPrm.lens_info.pixel_pitch);
		printf("planer_prm: ");
		for (int i = 0; i < 4; i++) {
			printf("%ld ", gPrm.lens_info.planer_prm[i]);
		}
		printf("\n");

		printf("distortion_prm: ");
		for (int i = 0; i < 4; i++) {
			printf("%ld ", gPrm.lens_info.distortion_prm[i]);
		}
		printf("\n");
	}

	ret = tl_enh_init(gPrm.handle, &tlprm);
	if (ret != TL_E_SUCCESS) {
		apl_print_error(ret, (char *)"tl_enh_init() failed", __LINE__);
		return -1;
	}

	gPrm.points_cloud = (int16_t*) malloc(gPrm.resolution.depth.width * gPrm.resolution.depth.height * sizeof(int16_t) * 3);
	if (gPrm.points_cloud == NULL) {
		printf("Point clound buffer allocate error\n");
		return -1;
	}

	return ret;
}


//******************************************************************************
//! \brief        Termination of libccdtof Library
//! \details
//! \param[in]    None
//! \param[out]   None
//! \return       0         success
//! \return       -1        failed
//******************************************************************************
static int apl_term(void)
{
	TL_E_RESULT ret;

	ret = TL_term(&gPrm.handle);
	if (ret != TL_E_SUCCESS) {
		apl_print_error(ret, (char *)"TL_term", __LINE__);
		return -1;
	}

	ret = tl_enh_term();
	if (ret != TL_E_SUCCESS) {
		apl_print_error(ret, (char *)"tl_enh_term", __LINE__);
		return -1;
	}

	return 0;
}


//******************************************************************************
//! \brief        Start Transferring
//! \details
//! \param[in]    None
//! \param[out]   None
//! \return       0         success
//! \return       -1        failed
//******************************************************************************
static int apl_start(void)
{
	TL_E_RESULT ret;

	ret = TL_start(gPrm.handle);
	if (ret != TL_E_SUCCESS) {
		apl_print_error(ret, (char *)"TL_start", __LINE__);
		return -1;
	}

	return 0;
}


//******************************************************************************
//! \brief        Capturing Of Images
//! \details
//! \param[in]    None
//! \param[out]   None
//! \return       0         success
//! \return       -1        failed
//******************************************************************************
static int apl_capture(void)
{
	TL_E_RESULT ret;
	uint32_t notify = 0U;
	TL_Image data;

	memset(&data, 0, sizeof(data));

	ret = TL_capture(gPrm.handle, &notify, &(data));

	if (ret == TL_E_SUCCESS) {
		apl_callback(gPrm.handle, notify, data);
	}
	else {
		apl_print_error(ret, (char *)"TL_capture", __LINE__);
	}

	return ret;
}


//******************************************************************************
//! \brief        Cancelation Of Capture
//! \details
//! \param[in]    None
//! \param[out]   None
//! \return       0         success
//! \return       -1        failed
//******************************************************************************
static int apl_cancel(void)
{
	TL_E_RESULT ret;

	ret = TL_cancel(gPrm.handle);
	if (ret == TL_E_SUCCESS) {
		printf("TL_cancel success\n");
	}
	else {
		printf("TL_cancel fail\n");
	}

	return ret;
}


//******************************************************************************
//! \brief        Stop Transferring
//! \details
//! \param[in]    None
//! \param[out]   None
//! \return       0         success
//! \return       -1        failed
//******************************************************************************
static int apl_stop(void)
{
	TL_E_RESULT ret;

	ret = TL_stop(gPrm.handle);
	if (ret != TL_E_SUCCESS) {
		apl_print_error(ret, (char *)"TL_stop", __LINE__);
		return -1;
	}

	return 0;
}


//******************************************************************************
//! \brief        Print Error From libccdtof.so Library
//! \details
//! \param[in]    ret           return value from tof library
//! \param[in]    function	    function of error happend
//! \param[in]    line          line of error happend
//! \param[out]   None
//! \return       None
//******************************************************************************
void apl_print_error(TL_E_RESULT ret, char *function, unsigned int line)
{
	switch (ret) {
		case TL_E_SUCCESS:
			break;
		case TL_E_ERR_PARAM:
			printf("[%s L.%d] TL_E_ERR_PARAM\n", function, (int)line);
			break;
		case TL_E_ERR_SYSTEM:
			printf("[%s L.%d] TL_E_ERR_SYSTEM\n", function, (int)line);
			break;
		case TL_E_ERR_STATE:
			printf("[%s L.%d] TL_E_ERR_STATE\n", function, (int)line);
			break;
		case TL_E_ERR_TIMEOUT:
			printf("[%s L.%d] TL_E_ERR_TIMEOUT\n", function, (int)line);
			break;
		case TL_E_ERR_EMPTY:
			printf("[%s L.%d] TL_E_ERR_EMPTY\n", function, (int)line);
			break;
		case TL_E_ERR_NOT_SUPPORT:
			printf("[%s L.%d] TL_E_ERR_NOT_SUPPORT\n", function, (int)line);
			break;
		case TL_E_ERR_CANCELED:
			printf("[%s L.%d] TL_E_ERR_CANCELED\n", function, (int)line);
			break;
		case TL_E_ERR_OTHER:
			printf("[%s L.%d] TL_E_ERR_OTHER\n", function, (int)line);
			break;
		default:
			printf("[%s L.%d] unknow error(%d)\n", function, (int)line, (int)ret);
			break;
	}
}


//******************************************************************************
//! \brief        Calculate Image Size From Pixel Format
//! \details
//! \param[in]    format	pixel format (tof library)
//! \param[out]   size      image size
//! \return       None
//******************************************************************************
void apl_calc_img_size(TL_ImageFormat *format, size_t *size)
{
	// stride = image_width * bit_per_pixel
	*size = (size_t)format->stride * (size_t)format->height;
}


//******************************************************************************
//! \brief        Calculate Data Size From Pixel Format
//! \details
//! \param[in]    format    pixel format (tof library)
//! \param[in]    bpp       bit per pixel of data
//! \param[out]   size      data size
//! \return       None
//******************************************************************************
void apl_calc_data_size(TL_ImageFormat *format, uint16_t bpp, size_t *size)
{
	*size = (size_t)format->width * (size_t)format->height * (size_t)bpp;
}


//******************************************************************************
//! \brief        Calculate Images Size
//! \details
//! \param[in]    None
//! \param[out]   None
//! \return       None
//******************************************************************************
void apl_images_size(void)
{
	switch (gPrm.image_kind) {
		case TL_E_IMAGE_KIND_VGA_DEPTH_QVGA_IR_BG:
		case TL_E_IMAGE_KIND_QVGA_DEPTH_IR_BG:
			apl_calc_img_size(&gPrm.resolution.depth, &gPrm.img_size.depth);
			apl_calc_img_size(&gPrm.resolution.ir,    &gPrm.img_size.ir);
			apl_calc_img_size(&gPrm.resolution.bg,    &gPrm.img_size.bg);
			break;
		case TL_E_IMAGE_KIND_VGA_DEPTH_IR:
		case TL_E_IMAGE_KIND_VGA_IR_QVGA_DEPTH:
			apl_calc_img_size(&gPrm.resolution.depth, &gPrm.img_size.depth);
			apl_calc_img_size(&gPrm.resolution.ir,    &gPrm.img_size.ir);
			break;
		case TL_E_IMAGE_KIND_VGA_IR_BG:
			apl_calc_img_size(&gPrm.resolution.ir, &gPrm.img_size.ir);
			apl_calc_img_size(&gPrm.resolution.bg, &gPrm.img_size.bg);
			break;
		default:
			break;
	}
}


//******************************************************************************
//! \brief        Handle Image Once Obtain From libccdtof.so
//! \details
//! \param[in]    handle    camera handle
//! \param[in]    notify    contents of the notification
//! \param[in]    data      Transfer data
//! \param[out]   None
//! \return       0         success
//! \return       -1        failed
//******************************************************************************
void apl_callback(TL_Handle *handle, uint32_t notify, TL_Image data)
{
	// Error Happened
	if ((notify & (uint32_t)TL_NOTIFY_NO_BUFFER)  != 0U) {
		printf("recv:TL_NOTIFY_NO_BUFFER\n");
	}

	if ((notify & (uint32_t)TL_NOTIFY_DISCONNECT) != 0U) {
		printf("recv:TL_NOTIFY_DISCONNECT\n");
	}

	if ((notify & (uint32_t)TL_NOTIFY_DEVICE_ERR) != 0U) {
		printf("recv:TL_NOTIFY_DEVICE_ERR\n");
	}

	if ((notify & (uint32_t)TL_NOTIFY_SYSTEM_ERR) != 0U) {
		printf("recv:TL_NOTIFY_SYSTEM_ERR\n");
	}

	if ((notify & (uint32_t)TL_NOTIFY_STOPPED)    != 0U) {
		printf("recv:TL_NOTIFY_STOPPED\n");
	}

	// recieved image data
	if ((notify & (uint32_t)TL_NOTIFY_IMAGE) != 0U) {
		apl_show_img(gPrm.mode, gPrm.image_kind, gPrm.resolution, &data);
	}
}


//******************************************************************************
//! \brief        Signal Handler Function
//! \details
//! \param[in]    signal    signal number
//! \param[out]   None
//! \return       None
//******************************************************************************
void apl_signal_handler(int signal)
{
	(void)signal;
	bExit = true;

	apl_cancel();
}


//******************************************************************************
//! \brief        Get Ranging Mode From User
//! \details
//! \param[in]    None
//! \param[out]   mode
//! \return       xxxx
//! \date         2021-02-16, Tue, 02:33 PM
//******************************************************************************
int apl_get_user_selection(TL_E_MODE *mode)
{
	char select_temp, temp;

	// Ranging mode selection
	printf("\n");
	printf("Ranging mode selection \n");

	do {
		printf( "0 : MODE0\n"
				"1 : MODE1\n");
		printf( "Your choice : ");
		if (!scanf("%c", &select_temp)) {
			printf("Failed to read\n");
			return -1;
		}

		temp = getchar();
		if (temp != '\n') {
			printf("Invalid parameter, please enter again.\n");
			int c;
			while ((c = getchar()) != '\n' && c != (char)EOF) {
				// Wait For Enter Key
			}
			continue;
		}
		else
		if (select_temp < '0' || select_temp > '1') {
			printf("Invalid parameter, please enter again.\n");
			printf("\n");
			continue;
		}
		break;
	} while (1);

	*mode = (TL_E_MODE) atoi(&select_temp);
	printf( "\n");

	return 0;
}


//******************************************************************************
//! \brief        Utilities Function To Convert Depth Image To Color Map, By Using OpenCV API.
//! \n
//! \param[in]    img       Depth Image.
//! \param[in]    min_val   Minimum Depth Value.
//! \param[in]    max_val   Depth Value.
//! \param[out]   None.
//! \return       cv::Mat of Type CV_8UC3 (8Bits 3 Channels).
//******************************************************************************
cv::Mat apl_dpth_to_color_by_opencv(cv::Mat img, uint32_t min_val, uint32_t max_val)
{
	size_t i;
	size_t j;
	size_t w = img.cols;
	size_t h = img.rows;
	double d_min_val = min_val;
	double d_max_val = max_val;
	cv::Mat mat_8bit(h, w, CV_8UC1);
	cv::Mat mat_32bit = cv::Mat::zeros(h, w, CV_32F);

	//! \remark 1. Normalise To 32Bits Float.
	img.convertTo(mat_32bit, CV_32FC1, 1/(d_max_val - d_min_val), -d_min_val/(d_max_val - d_min_val));

	//! \remark 2. Convert To 8Bits, As applyColorMap() Only Accept CV_8U.
	mat_32bit.convertTo(mat_8bit, CV_8UC1, -255, 255);

	//! \remark 3. Convert To Rainbow Color.
	cv::applyColorMap(mat_8bit, mat_8bit, cv::COLORMAP_JET);

	//! \remark 4. Mask Off Upper And Lower Range.
	for (i = 0 ; i < h; i++) {
		for (j = 0 ; j < w; j++) {
			float y = mat_32bit.at<float>(i,j);
			if (y > 1) {
				mat_8bit.at<cv::Vec3b>(i,j) = 0;	// Change To Black, i.e. 0 if (img > 1).
			}
			else
			if (y < 0) {
				mat_8bit.at<cv::Vec3b>(i,j) = 255;	// Change To White, i.e. 255 if (img < 0).
			}
		}
	}

	return mat_8bit;
}


//******************************************************************************
//! \brief        Display Image In Opencv Windows
//! \n
//! \param[in]    img_kind      Image data.
//! \param[in]    reso          Depth Image Format.
//! \param[in]    stData        Image data.
//! \param[in]    temperature   Temperature.
//! \param[out]   None.
//! \return       None
//******************************************************************************
void apl_show_img(TL_E_MODE mode, TL_E_IMAGE_KIND img_kind, TL_Resolution reso, TL_Image *stData)
{
	bool show_depth = false;
	bool show_ir = false;
	bool show_bg = false;
	bool show_ptcd = true;
	size_t w;
	size_t h;
	uint8_t *p_data;
	uint16_t range_min;
	uint16_t range_max;
	static int32_t gamma_corr_ir = 22;  //!< Default Gamma Value Is 2.2 (For OpenCV TrackBar Used).
	static int32_t gamma_corr_bg = 22;  //!< Default Gamma Value Is 2.2 (For OpenCV TrackBar Used).
	int32_t temperature;
	char str[256];

	switch (img_kind) {
		case TL_E_IMAGE_KIND_VGA_DEPTH_QVGA_IR_BG:
		case TL_E_IMAGE_KIND_QVGA_DEPTH_IR_BG:
			show_depth = true;
			show_ir = true;
			show_bg = true;
			break;

		case TL_E_IMAGE_KIND_VGA_DEPTH_IR:
		case TL_E_IMAGE_KIND_VGA_IR_QVGA_DEPTH:
			show_depth = true;
			show_ir = true;
			show_bg = false;
			break;

		case TL_E_IMAGE_KIND_VGA_IR_BG:
			show_depth = false;
			show_ir = true;
			show_bg = true;
			break;

		default:
			break;
	}

	if (show_depth) {
		// --------------------------------------------------
		//! \remark - Process Depth Image
		//! \remark - Obtain Height, Width, Pointer To Data From Toflib-Output Message.
		h = reso.depth.height;
		w = reso.depth.width;
		p_data = (uint8_t *)stData->depth;

		if (show_ptcd) {
			//! \remark - Convert Depth To 3D.
			tl_enh_convert_camera_coord(gPrm.handle, (uint16_t *)(stData->depth), &gPrm.points_cloud);

			//! \remark - Update Point Cloud Data.
			int32_t ptCdCnt = gPrm.resolution.depth.width * gPrm.resolution.depth.height;
			uint64_t ns = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
			update3dData(ns, gPrm.points_cloud, ptCdCnt);
		}

		//! \remark - Create Cv Matrix, 16 Bits.
		cv::Mat mat_depth_raw(h, w, CV_16UC1, p_data);

		//! \remark - Decide The Range For Depth Base On Range Mode.
		range_min = gPrm.mode_info_grp.mode[gPrm.mode].range_near;
		range_max = gPrm.mode_info_grp.mode[gPrm.mode].range_far;

		//! \remark - Depth To Color Conversion, Using OpenCV API.
		cv::Mat mat_depth_color_by_opencv;
		mat_depth_color_by_opencv = apl_dpth_to_color_by_opencv(mat_depth_raw, range_min, range_max);

		//! \remark - Add Temperature Text.
		temperature = stData->temp;
		std::snprintf(str, sizeof(str), "temperature=%d.%d C", temperature/100, temperature%100);
		cv::putText(mat_depth_color_by_opencv, std::string(str), cv::Point(10, 20), cv::FONT_HERSHEY_COMPLEX_SMALL, 0.6, cv::Scalar(255, 255, 255), 1, cv::LINE_AA);

		//! \remark - Display It.
		cv::imshow(OPENCV_WINDOW_NAME_DPTH, mat_depth_color_by_opencv);
		cv::waitKey(1);	// Draw The Screen And Wait For 1 Millisecond.
	}

	if (show_ir) {
		// --------------------------------------------------
		//! \remark - Proecess IR Image
		//! \remark - Obtain Height, Width, Pointer To Data From Toflib-Output Message.
		h = reso.ir.height;
		w = reso.ir.width;
		p_data = (uint8_t *)stData->ir;

		//! \remark - Create Cv Matrix, 16 Bits.
		cv::Mat mat_ir(h, w, CV_16UC1, p_data);

		//! \remark - Convert To Double For "pow()".
		cv::Mat mat_ir_64f;
		mat_ir.convertTo(mat_ir_64f, CV_64F);

		//! \remark - Apply Gamma Correction.
		cv::Mat mat_ir_pow;
		cv::pow(mat_ir_64f, (float) gamma_corr_ir/10, mat_ir_pow);

		//! \remark - Convert Back To 16Bits.
		mat_ir_pow.convertTo(mat_ir, CV_16UC1);

		//! \remark - Add Temperature Text.
		temperature = stData->temp;
		std::snprintf(str, sizeof(str), "temperature=%d.%d C", temperature/100, temperature%100);
		cv::putText(mat_ir, std::string(str), cv::Point(10, 20), cv::FONT_HERSHEY_COMPLEX_SMALL, 0.6, cv::Scalar(255, 255, 255), 1, cv::LINE_AA);

		//! \remark - Display It.
		cv::createTrackbar(OPENCV_TRACKBAR_NAME_GAMMA_CORR_IR, OPENCV_WINDOW_NAME_IR, &gamma_corr_ir, 30);
		cv::imshow(OPENCV_WINDOW_NAME_IR, mat_ir);
		cv::waitKey(1);	// Draw The Screen And Wait For 1 Millisecond.
	}

	if (show_bg) {
		// --------------------------------------------------
		//! \remark - Proecess BG Image
		//! \remark - Obtain Height, Width, Pointer To Data From Toflib-Output Message.
		h = reso.bg.height;
		w = reso.bg.width;
		p_data = (uint8_t *)stData->bg;

		//! \remark - Create Cv Matrix, 16 Bits.
		cv::Mat mat_bg(h, w, CV_16UC1, p_data);

		//! \remark - Convert To Double For "pow()".
		cv::Mat mat_bg_64f;
		mat_bg.convertTo(mat_bg_64f, CV_64F);

		//! \remark - Apply Gamma Correction.
		cv::Mat mat_bg_pow;
		cv::pow(mat_bg_64f, (float) gamma_corr_bg/10, mat_bg_pow);

		//! \remark - Convert Back To 16Bits.
		mat_bg_pow.convertTo(mat_bg, CV_16UC1);

		//! \remark - Display It.
		cv::createTrackbar(OPENCV_TRACKBAR_NAME_GAMMA_CORR_BG, OPENCV_WINDOW_NAME_BG, &gamma_corr_bg, 30);
		cv::imshow(OPENCV_WINDOW_NAME_BG, mat_bg);
		cv::waitKey(1);	// Draw The Screen And Wait For 1 Millisecond.
	}
}


//******************************************************************************
//! \brief        Thread to handle image capture and view
//! \n
//! \param[in]    data         Data.
//! \return       void pointer
//******************************************************************************
void *view_thread(void *data)
{
	while (!bExit) {
		apl_capture();
	}

	//apl_cancel();
	mainPtCloudViewExit();
}


//******************************************************************************
//! \brief        Thread to handle 3D image point clouds
//! \n
//! \param[in]    data         Data.
//! \return       void pointer
//******************************************************************************
void *view_3d_thread(void *data)
{
	while (!bExit) {
		mainPtCloudView(30, 9000, OPENGL_WINDOW_NAME_PTCD);
	}
}


//******************************************************************************
//! \brief        main function
//! \n
//! \param[in]    argc         number of arguments.
//! \param[in]    argv         arguments.
//! \return       -1           fail with some error.
//******************************************************************************
int main(int argc, char *argv[])
{
	int ret = 0;
	TL_E_MODE mode;
	TL_E_IMAGE_KIND image_kind;

	printf("\n");
	printf("----------------------------------------\n");
	printf("Viewer [ver%04x]\n", (int)VIEWER_VERSION);

	memset(&gPrm, 0, sizeof(gPrm));

	signal(SIGINT, apl_signal_handler);

	// Get user input selection
	if (apl_get_user_selection(&mode) != 0) {
		printf ("getUserSelection failed\n");
	}
	image_kind = TL_E_IMAGE_KIND_VGA_DEPTH_IR;


	if ((ret = apl_init(mode, image_kind)) < 0) {
		printf("apl_init failed\n");
		exit(-1);
	}

	apl_images_size();

	if (apl_start() < 0) {
		printf ("apl_start failed\n");
		(void) apl_term();
		exit(-1);
	}

	// Create Threads
	if (pthread_create(&threadview, NULL, view_thread, NULL) != 0) {
		printf("pthread_create failed\n");
		exit(-1);
	}

	if (pthread_create(&threadview3d, NULL, view_3d_thread, NULL) != 0) {
		printf("pthread_create failed\n");
		exit(-1);
	}

	printf("\n");
	printf("Press [ctrl + c] to quit. \n");

	// Spin Here
	while (!bExit) {
		sleep(100);
	}

	if (apl_stop() < 0) {
		printf("app exit abnormal\n");
		(void) apl_term();
		exit(-1);
	}

	if (apl_term() < 0) {
		printf("apl_term abnormal\n");
		exit(-1);
	}

	// Wait Threads Terminate
	if (threadview) {
		pthread_join(threadview, NULL);
	}

	if (threadview3d) {
		pthread_join(threadview3d, NULL);
	}

	printf("viewer exited\n\n");
	printf("----------------------------------------\n");

	return 0;
}

