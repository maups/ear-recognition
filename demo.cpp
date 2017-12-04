#include <bits/stdc++.h>
#include <opencv2/dnn.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>

using namespace cv;
using namespace cv::dnn;
using namespace std;

/* --------------------------------------------------------------------------------------- */
/* Description: checks if a cropped ear image is left oriented                             */
/* Parameters:                                                                             */
/*     Mat img: 96x96 image with a cropped ear                                             */
/*              - left or right oriented                                                   */
/*              - with or without pose variation                                           */
/* Return: true if ear is left oriented, false otherwise                                   */
/* --------------------------------------------------------------------------------------- */
bool isLeft(Mat img) {
	static bool first = true;
	static Net net;

	if(first) {
		first = false;
		net = readNetFromTensorflow("model-side.pb");
		if(net.empty()) {
			cerr << "ERROR: Could not load the CNN for side classification" << endl;
			exit(1);
		}
	}

	Mat inputBlob = blobFromImage(img);
	inputBlob /= 255.0;
	net.setInput(inputBlob, "Placeholder");
	Mat result = net.forward("side_/out/MatMul");

	return result.at<float>(0,0) > result.at<float>(0,1);
}

/* --------------------------------------------------------------------------------------- */
/* Description: detects landmarks in a left oriented cropped ear image                     */
/* Parameters:                                                                             */
/*     Mat img: 96x96 image with a cropped ear                                             */
/*              - left oriented                                                            */
/*              - with or without pose variation                                           */
/*     vector<Point2d> &ldmk: vector that receives the 2d coordinates of 55 landmarks      */
/*     stage s: FIRST locates landmarks in ears with large pose variations                 */
/*              SECOND locates landmarks in ears with coarse pose normalization            */
/* Return: none                                                                            */
/* --------------------------------------------------------------------------------------- */
typedef enum __stage {FIRST, SECOND} stage;
void detectLandmarks(Mat img, vector<Point2d> &ldmk, stage s) {
	static bool first = true;
	static Net net1, net2;

	if(first) {
		first = false;
		net1 = readNetFromTensorflow("model-stage1.pb");
		net2 = readNetFromTensorflow("model-stage2.pb");
		if(net1.empty() || net2.empty()) {
			cerr << "ERROR: Could not load the CNNs for landmark detection" << endl;
			exit(1);
		}
	}

	Mat result, inputBlob = blobFromImage(img);
	inputBlob /= 255.0;
	if(s == FIRST) {
		net1.setInput(inputBlob);
		result = net1.forward("ear_ang45_3_sca20_r_tra20_r_e/out/MatMul");
	}
	else {
		net2.setInput(inputBlob);
		result = net2.forward("ear_ang45_3_sca20_r_tra20_r_e/out/MatMul");
	}
	result *= 48;
	result += 48;

	ldmk.clear();
	for(int i=0; i < 55; i++)
		ldmk.push_back(Point2d(result.at<float>(0,i*2), result.at<float>(0,i*2+1)));
}

/* --------------------------------------------------------------------------------------- */
/* Description: extracts CNN descriptor from a left oriented, pose normalized ear image    */
/* Parameters:                                                                             */
/*     Mat img: 128x128 image with a normalized ear                                        */
/*              - left oriented                                                            */
/*              - without pose variation                                                   */
/*              - ear height is equal to ear width                                         */
/* Return: Mat with a 512-dimensional descriptor                                           */
/* --------------------------------------------------------------------------------------- */
Mat extractDescriptor(Mat img) {
	static bool first = true;
	static Net net;

	if(first) {
		first = false;
		net = readNetFromTensorflow("model-descriptor.pb");
		if(net.empty()) {
			cerr << "ERROR: Could not load the CNN for side classification" << endl;
			exit(1);
		}
	}

	Mat inputBlob = blobFromImage(img);
	inputBlob /= 255.0;
	net.setInput(inputBlob);
	Mat result = net.forward("MatMul");

	return result.clone();
}

/* --------------------------------------------------------------------------------------- */
/* Description: interpolate ear image using ear center, orientation and scale              */
/* Parameters:                                                                             */
/*     Mat image: image with any size of a cropped ear                                     */
/*     Mat &output: output interpolated image                                              */
/*     int size: output dimensions are size x size                                         */
/*     double scaley: vertical radius of the ear in the input image (in pixels)            */
/*     double scalex: horizontal radius of the ear in the input image (in pixels)          */
/*     double ang: ear rotation in radians                                                 */
/*     double cx: center of the ear in the horizontal axis                                 */
/*     double cy: center of the ear in the vertical axis                                   */
/* Return: none                                                                            */
/* --------------------------------------------------------------------------------------- */
void normalizeImage(Mat image, Mat &output, int size, double scaley, double scalex, double ang, double cx, double cy) {
	output.create(size, size, CV_8UC1);
	double ratioy = (scaley/((size-1)/2.0)), ratiox = (scalex/((size-1)/2.0));
	for(int i=0; i < size; i++)
		for(int j=0; j < size; j++) {
			double xt = ratiox*(j-(size-1)/2.0), yt = ratioy*(i-(size-1)/2.0);
			double x = xt*cos(ang)-yt*sin(ang)+cx, y = xt*sin(ang)+yt*cos(ang)+cy;
			int u = x, v = y;
			double ul = x-u, vl = y-v;
			int u1 = u+1, v1 = v+1;
			u = max(0, min(image.cols-1, u));
			u1 = max(0, min(image.cols-1, u1));
			v = max(0, min(image.rows-1, v));
			v1 = max(0, min(image.rows-1, v1));

			double tmp = image.at<uchar>(v,u)*(1.0-ul)*(1.0-vl) + image.at<uchar>(v,u1)*ul*(1.0-vl) + image.at<uchar>(v1,u)*(1.0-ul)*vl + image.at<uchar>(v1,u1)*ul*vl;
			output.at<uchar>(i,j) = max(0.0,min(255.0,tmp));	
		}
}

/* --------------------------------------------------------------------------------------- */
/* Description: adjust pose parameters using landmarks                                     */
/* Parameters:                                                                             */
/*     vector<Point2d> ldmk: 2d coordinates of 55 landmarks                                */
/*     int size: size of the interpolated image from which the landmarks were extracted    */
/*     int size: output dimensions are size x size                                         */
/*     double &scaley: vertical radius of the ear in the input image (in pixels)           */
/*     double &scalex: horizontal radius of the ear in the input image (in pixels)         */
/*     double &ang: ear rotation in radians                                                */
/*     double &cx: center of the ear in the horizontal axis                                */
/*     double &cy: center of the ear in the vertical axis                                  */
/*     bool oriented: true if new center must use oriented bounding box, false otherwise   */
/*     bool scaled: true if interpolated image was generated using different values for    */
/*                  scaley and scalex, false otherwise                                     */
/* Observation: scaley, scalex, ang, cx and cy must contain the values used to generate    */
/*              the interpolated image, and will be overwritten with the new values        */
/* Return: none                                                                            */
/* --------------------------------------------------------------------------------------- */
void adjustParameters(vector<Point2d> ldmk, int size, double &scaley, double &scalex, double &ang, double &cx, double &cy, bool oriented, bool scaled) {
	double ratioy = (scaley/((size-1)/2.0)), ratiox = ((scaled ? scalex : scaley)/((size-1)/2.0));

	/* align landmark coordinate space to the original image */
	/* compute principal components and bounding box for aligned landmarks */
	Mat data_pts = Mat(55, 2, CV_64FC1);
	for(int i=0; i < 55; i++) {
		double xt = ratiox*(ldmk[i].x-(size-1)/2.0), yt = ratioy*(ldmk[i].y-(size-1)/2.0);
		data_pts.at<double>(i, 0) = xt*cos(ang)-yt*sin(ang)+cx;
		data_pts.at<double>(i, 1) = xt*sin(ang)+yt*cos(ang)+cy;
	}
	PCA pca_analysis(data_pts, Mat(), CV_PCA_DATA_AS_ROW);

	/* set orientation of the ear as the direction of the first principal component */
	double angle = atan2(pca_analysis.eigenvectors.at<double>(0, 1), pca_analysis.eigenvectors.at<double>(0, 0));
	while(angle < 0.0)
		angle += 2.0*M_PI;
	if(angle > M_PI)
		angle -= M_PI;
	ang = angle-M_PI/2.0;

	/* set scale as two times the deviation of the first principal component */
	scaley = 2.0*sqrt(pca_analysis.eigenvalues.at<double>(0, 0));
	scalex = 2.0*sqrt(pca_analysis.eigenvalues.at<double>(0, 1));

	if(!oriented) {
		/* set center as the center of the bounding box */
		Point2d tl = {DBL_MAX, DBL_MAX}, br = {-DBL_MAX, -DBL_MAX};
		for(int i=0; i < 55; i++) {
			tl.x = min(tl.x, data_pts.at<double>(i, 0));
			tl.y = min(tl.y, data_pts.at<double>(i, 1));
			br.x = max(br.x, data_pts.at<double>(i, 0));
			br.y = max(br.y, data_pts.at<double>(i, 1));
		}
		cx = (tl.x+br.x)/2.0;
		cy = (tl.y+br.y)/2.0;
	}
	else {
		/* set center as the center of the oriented bounding box */
		Mat norm_pts = pca_analysis.project(data_pts);
		double top=-DBL_MAX, bottom=DBL_MAX, left=DBL_MAX, right=-DBL_MAX;
		for(int i=0; i < 55; i++) {
			top = max(top, norm_pts.at<double>(i, 0));
			bottom = min(bottom, norm_pts.at<double>(i, 0));
			right = max(right, norm_pts.at<double>(i, 1));
			left = min(left, norm_pts.at<double>(i, 1));
		}
		cx = (bottom+top)/2.0*pca_analysis.eigenvectors.at<double>(0, 0) + (left+right)/2.0*pca_analysis.eigenvectors.at<double>(1, 0) + pca_analysis.mean.at<double>(0, 0);
		cy = (bottom+top)/2.0*pca_analysis.eigenvectors.at<double>(0, 1) + (left+right)/2.0*pca_analysis.eigenvectors.at<double>(1, 1) + pca_analysis.mean.at<double>(0, 1);
	}
}

/* magic function */
int main(int argc, char **argv) {
	/* check if an image name was provided as argument */
	if(argc != 2) {
		cerr << "Usage:" << argv[0] << " filename.{png|jpg|...}" << endl;
		return 1;
	}

	/* load cropped ear image */
	Mat image = imread(argv[1], IMREAD_GRAYSCALE), interpolated;





	/* image size for side classification and landmark detection */
	const int PREPROC_SIZE = 96;
	/* image size for cnn description */
	const int DESCRIPT_SIZE = 128;
	/* set largest image axis as initial scale */
	double scaley = (max(image.rows, image.cols)-1.0)/2.0, scalex;
	/* initial orientation unknown */
	double ang = 0.0;
	/* use center of the image as initial ear center */
	double cx = (image.cols-1.0)/2.0, cy = (image.rows-1.0)/2.0;
	/* vector for 2d coordinates of ear landmarks */
	vector<Point2d> landmarks;

	/* normalize image using initial guesses for ear location, size and orientation */
	/* use the same scale for x and y axes */
	normalizeImage(image, interpolated, PREPROC_SIZE, scaley, scaley, ang, cx, cy);

	/* check if the ear is left-oriented and flip it if it is not */
	if(!isLeft(interpolated)) {
		Mat tmp;
		flip(interpolated, tmp, 1);
		tmp.copyTo(interpolated);
		flip(image, tmp, 1);
		tmp.copyTo(image);
	}

	/* detect landmarks using stage 1 (robust to intense variations) */
	detectLandmarks(interpolated, landmarks, FIRST);

	/* normalize image using adjusted parameters */
	/* use the same scale for x and y axes */
	adjustParameters(landmarks, PREPROC_SIZE, scaley, scalex, ang, cx, cy, false, false);
	normalizeImage(image, interpolated, PREPROC_SIZE, scaley, scaley, ang, cx, cy);

	/* detect landmarks using stage 2 (fine tuned to small variations) */
	detectLandmarks(interpolated, landmarks, SECOND);

	/* normalize image using adjusted parameters */
	/* use different scales for x and y axes */
	adjustParameters(landmarks, PREPROC_SIZE, scaley, scalex, ang, cx, cy, true, false);
	normalizeImage(image, interpolated, DESCRIPT_SIZE, scaley, scalex, ang, cx, cy);

	/* extract discriminant descriptor */
	Mat descriptor = extractDescriptor(interpolated);





	/* save normalized image and CNN descriptor */
	imwrite("output.png", interpolated);
	ofstream fp("output.txt", ofstream::out);
	for(int i=0; i < 512; i++)
		fp << descriptor.at<float>(0,i) << " ";
	fp << endl;
	fp.close();

	return 0;
}

