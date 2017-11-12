//2017.11.10
// demo_4.cpp : 定义控制台应用程序的入口点.
//功能:获取身份证的外矩形框边缘，并通过仿射变换对倾斜、变形的图片进行校正，投影到正面
//==========================================================
#include "stdafx.h"
#include <opencv2/core/core.hpp>  
#include <opencv2/imgproc/imgproc.hpp> 
#include <opencv2/highgui/highgui.hpp>  
#include <iostream>  
#include <map>
#include "opencv/cv.h"  
#include "opencv/highgui.h"  
#include <opencv2/opencv.hpp>  
#include <tchar.h>  
#include <iostream>  
#include <fstream> 
#include <string>
#include <math.h>
#include <windows.h>
#include <atlstr.h>
using namespace std;
using namespace cv;
//对二值化图进行反转
Mat reverse(Mat biImage)
{
	Mat result_3 = biImage < 100;
	return result_3;
}
//将UTF-8编码的中文字符转换为ASNI编码格式
//原因：因为通过Tesseract-OCR识别的文字或者数字是以UTF8编码格式存储到txt中，这里为了输出时不发生乱码，先转换成ANSI格式再输出。
void UTF8toANSI(CString &strUTF8)  
{
	//获取转换为多字节后需要的缓冲区大小，创建多字节缓冲区  
	UINT nLen = MultiByteToWideChar(CP_UTF8, NULL, strUTF8, -1, NULL, NULL);
	WCHAR *wszBuffer = new WCHAR[nLen + 1];
	nLen = MultiByteToWideChar(CP_UTF8, NULL, strUTF8, -1, wszBuffer, nLen);
	wszBuffer[nLen] = 0;

	nLen = WideCharToMultiByte(936, NULL, wszBuffer, -1, NULL, NULL, NULL, NULL);
	CHAR *szBuffer = new CHAR[nLen + 1];
	nLen = WideCharToMultiByte(936, NULL, wszBuffer, -1, szBuffer, nLen, NULL, NULL);
	szBuffer[nLen] = 0;

	strUTF8 = szBuffer;
	//清理内存  
	delete[]szBuffer;
	delete[]wszBuffer;
}
//===================================================
int main()
{
	string src_path;   //原始图片路径
	int src_width, src_height;
	cout << "输入图片路径：";
	cin >> src_path;
	Mat src = imread(src_path);
	src_width = src.size().width;
	src_height = src.size().height;

    //使用mean_shift函数，突出前景和背景的差异，同时减少纹理，为后面提取轮廓做准备
	Mat mean_shift;
	pyrMeanShiftFiltering(src, mean_shift, 20, 20, 3);  //原始:30 , 40 , 3
	imwrite("mean_shift.jpg", mean_shift);

    //转换为灰度图
	Mat gray_image;
	cvtColor(mean_shift, gray_image, CV_BGR2GRAY);
	imwrite("gray.jpg", gray_image);

	//二值化
	Mat biImage;
	adaptiveThreshold(gray_image, biImage, 255, CV_ADAPTIVE_THRESH_MEAN_C,
		CV_THRESH_BINARY_INV, 15, 10);  ///局部自适应二值化函数  
	imwrite("biImage.jpg", biImage);
    
    //contourImage初始化一个和原始图片相同大小的图像矩阵，灰度值为0
	Mat contourImage(biImage.rows, biImage.cols, CV_8UC1, Scalar(0, 0, 0));
	vector<vector<Point> > contours;
	vector<Vec4i> hierarchy;

	//找出轮廓，上面的点存储在contours中
	findContours(biImage, contours, hierarchy, CV_RETR_EXTERNAL, CV_CHAIN_APPROX_SIMPLE);
	for (int i = 0; i < contours.size(); i++)
	{
		drawContours(contourImage, contours, i, Scalar(255, 255, 255), 1, 8, hierarchy, 1);//绘制轮廓
	}
	imwrite("lunkuo.jpg", contourImage);
    
    //利用HoughLinesP（）画出轮廓中的直线
	vector<Vec4i>lines;
	HoughLinesP(contourImage, lines, 1, CV_PI / 180, 60, 60, 10);  //原始值是1,CV_PI/180,50,50,10
	Mat img_lines;
	cvtColor(contourImage, img_lines, CV_GRAY2BGR);
	for (size_t i = 0; i < lines.size(); i++)
	{
		Vec4i I = lines[i];
		line(img_lines, Point(I[0], I[1]), Point(I[2], I[3]), Scalar(0, 0, 255), 3, CV_AA);
	}
	imwrite("lunkuo_line.jpg", img_lines);

    //下面四个变量分别存储矩形框轮廓四个边上的直线段
	vector<Vec4i>up_ping_lines;
	vector<Vec4i>down_ping_lines;
	vector<Vec4i>left_shu_lines;
	vector<Vec4i>right_shu_lines;
	//int a = 0 , b = 0 , c = 0 , d = 0;
	for (size_t i = 0; i < lines.size(); i++)
	{
		Vec4i I = lines[i];
		//这里的阈值均设为60
		if ((abs(I[2] - I[0]) > 60) && (abs(I[3] - I[1]) < 60))  //水平直线
		{
			if (I[1] < (src_height / 2))
				up_ping_lines.push_back(I);
			else
				down_ping_lines.push_back(I);
		}
		else if (abs(I[2] - I[0] < 60) && (abs(I[3] - I[1]) > 60))  //竖直直线
		{
			if (I[0] < (src_width / 2))
				left_shu_lines.push_back(I);
			else
				right_shu_lines.push_back(I);
		}
			
	}

	// =================找出有倾斜的图片矩形框的四个顶点，之后再采用仿射变换进行矫正=========================
	// 选取方法是：例如对于左上角的顶点，它的坐标值等于和它相邻的最近的水平直线段和竖直直线段的交点，其他三个顶点类似；
	int left_up_x = 0, left_up_y = 0, right_up_x = 0, right_up_y = 0 , left_down_x = 0 , left_down_y = 0 , right_down_x = 0 , right_down_y = 0;
	float x_1, x_2, x_3, x_4, y_1, y_2, y_3, y_4;
	float a, b, c, d;
	//======================左上角坐标值=====================================
	if (up_ping_lines.size() == 1)
	{
		x_1 = up_ping_lines[0][0];
		x_2 = up_ping_lines[0][2];
		y_1 = up_ping_lines[0][1];
		y_2 = up_ping_lines[0][3];
	}
	else
	{
		x_1 = up_ping_lines[0][0];
		x_2 = up_ping_lines[0][2];
		y_1 = up_ping_lines[0][1];
		y_2 = up_ping_lines[0][3];
		for (size_t i = 0; i < up_ping_lines.size() - 1; i++)
		{
			if (x_1 >= up_ping_lines[i + 1][0])
			{
				x_1 = up_ping_lines[i + 1][0];
				x_2 = up_ping_lines[i + 1][2];
				y_1 = up_ping_lines[i + 1][1];
				y_2 = up_ping_lines[i + 1][3];
			}
		}
	}
	if (left_shu_lines.size() == 1)
	{
		x_3 = left_shu_lines[0][0];
		x_4 = left_shu_lines[0][2];
		y_3 = left_shu_lines[0][1];
		y_4 = left_shu_lines[0][3];
	}
	else
	{
		x_3 = left_shu_lines[0][0];
		x_4 = left_shu_lines[0][2];
		y_3 = left_shu_lines[0][1];
		y_4 = left_shu_lines[0][3];
		for (size_t i = 0; i < left_shu_lines.size() - 1; i++)
		{
			if (y_3 >= left_shu_lines[i + 1][1])
			{
				x_3 = left_shu_lines[i + 1][0];
				x_4 = left_shu_lines[i + 1][2];
				y_3 = left_shu_lines[i + 1][1];
				y_4 = left_shu_lines[i + 1][3];
			}
		}
	}
	a = (y_2 - y_1) / (x_2 - x_1);
	b = (x_2 * y_1 - x_1 * y_2) / (x_2 - x_1);
	if (x_3 == x_4)  //求左上角坐标值
	{
		left_up_x = x_3;
		left_up_y = a * x_3 + b;   
	}
	else
	{
		c = (y_4 - y_3) / (x_4 - x_3);
		d = (x_4 * y_3 - x_3 * y_4) / (x_4 - x_3);
		left_up_x = (d - b) / (a - c);
		left_up_y = (a * d - b * c) / (a - c);     
	}
//========================================================右上角坐标值============================
	if (up_ping_lines.size() == 1)
	{
		x_1 = up_ping_lines[0][0];
		x_2 = up_ping_lines[0][2];
		y_1 = up_ping_lines[0][1];
		y_2 = up_ping_lines[0][3];
	}
	else
	{
		x_1 = up_ping_lines[0][0];
		x_2 = up_ping_lines[0][2];
		y_1 = up_ping_lines[0][1];
		y_2 = up_ping_lines[0][3];
		for (size_t i = 0; i < up_ping_lines.size() - 1; i++)
		{
			if (x_1 <= up_ping_lines[i + 1][0])
			{
				x_1 = up_ping_lines[i + 1][0];
				x_2 = up_ping_lines[i + 1][2];
				y_1 = up_ping_lines[i + 1][1];
				y_2 = up_ping_lines[i + 1][3];
			}
		}
	}
	if (right_shu_lines.size() == 1)
	{
		x_3 = right_shu_lines[0][0];
		x_4 = right_shu_lines[0][2];
		y_3 = right_shu_lines[0][1];
		y_4 = right_shu_lines[0][3];
	}
	else
	{
		x_3 = right_shu_lines[0][0];
		x_4 = right_shu_lines[0][2];
		y_3 = right_shu_lines[0][1];
		y_4 = right_shu_lines[0][3];
		for (size_t i = 0; i < right_shu_lines.size() - 1; i++)
		{
			if (y_3 >= right_shu_lines[i + 1][1])
			{
				x_3 = right_shu_lines[i + 1][0];
				x_4 = right_shu_lines[i + 1][2];
				y_3 = right_shu_lines[i + 1][1];
				y_4 = right_shu_lines[i + 1][3];
			}
		}
	}
	a = (y_2 - y_1) / (x_2 - x_1);
	b = (x_2 * y_1 - x_1 * y_2) / (x_2 - x_1);
	if ((x_4 - x_3) == 0)  //求左上角坐标值
	{
		right_up_x = x_3;
		right_up_y = a * x_3 + b;
	}
	else
	{
		c = (y_4 - y_3) / (x_4 - x_3);
		d = (x_4 * y_3 - x_3 * y_4) / (x_4 - x_3);
		right_up_x = (d - b) / (a - c);
		right_up_y = (a * d - b * c) / (a - c);
	}
	//======================================================左下角坐标值=============================================
	if (down_ping_lines.size() == 1)
	{
		x_1 = down_ping_lines[0][0];
		x_2 = down_ping_lines[0][2];
		y_1 = down_ping_lines[0][1];
		y_2 = down_ping_lines[0][3];
	}
	else
	{
		x_1 = down_ping_lines[0][0];
		x_2 = down_ping_lines[0][2];
		y_1 = down_ping_lines[0][1];
		y_2 = down_ping_lines[0][3];
		for (size_t i = 0; i < down_ping_lines.size() - 1; i++)
		{
			if (x_1 >= down_ping_lines[i + 1][0])
			{
				x_1 = down_ping_lines[i + 1][0];
				x_2 = down_ping_lines[i + 1][2];
				y_1 = down_ping_lines[i + 1][1];
				y_2 = down_ping_lines[i + 1][3];
			}
		}
	}
	if (left_shu_lines.size() == 1)
	{
		x_3 = left_shu_lines[0][0];
		x_4 = left_shu_lines[0][2];
		y_3 = left_shu_lines[0][1];
		y_4 = left_shu_lines[0][3];
	}
	else
	{
		x_3 = left_shu_lines[0][0];
		x_4 = left_shu_lines[0][2];
		y_3 = left_shu_lines[0][1];
		y_4 = left_shu_lines[0][3];
		for (size_t i = 0; i < left_shu_lines.size() - 1; i++)
		{
			if (y_3 <= left_shu_lines[i + 1][1])
			{
				x_3 = left_shu_lines[i + 1][0];
				x_4 = left_shu_lines[i + 1][2];
				y_3 = left_shu_lines[i + 1][1];
				y_4 = left_shu_lines[i + 1][3];
			}
		}
	}
	a = (y_2 - y_1) / (x_2 - x_1);
	b = (x_2 * y_1 - x_1 * y_2) / (x_2 - x_1);
	if ((x_4 - x_3) == 0)  //求左下角坐标值
	{
		left_down_x = x_3;
		left_down_y = a * x_3 + b;
	}
	else
	{
		c = (y_4 - y_3) / (x_4 - x_3);
		d = (x_4 * y_3 - x_3 * y_4) / (x_4 - x_3);
		left_down_x = (d - b) / (a - c);
		left_down_y = (a * d - b * c) / (a - c);
	}
	//=====================================右下角坐标值=====================================
	if (down_ping_lines.size() == 1)
	{
		x_1 = down_ping_lines[0][0];
		x_2 = down_ping_lines[0][2];
		y_1 = down_ping_lines[0][1];
		y_2 = down_ping_lines[0][3];
	}
	else
	{
		x_1 = down_ping_lines[0][0];
		x_2 = down_ping_lines[0][2];
		y_1 = down_ping_lines[0][1];
		y_2 = down_ping_lines[0][3];
		for (size_t i = 0; i < down_ping_lines.size() - 1; i++)
		{
			if (x_1 <= down_ping_lines[i + 1][0])
			{
				x_1 = down_ping_lines[i + 1][0];
				x_2 = down_ping_lines[i + 1][2];
				y_1 = down_ping_lines[i + 1][1];
				y_2 = down_ping_lines[i + 1][3];
			}
		}
	}
	if (right_shu_lines.size() == 1)
	{
		x_3 = right_shu_lines[0][0];
		x_4 = right_shu_lines[0][2];
		y_3 = right_shu_lines[0][1];
		y_4 = right_shu_lines[0][3];
	}
	else
	{
		x_3 = right_shu_lines[0][0];
		x_4 = right_shu_lines[0][2];
		y_3 = right_shu_lines[0][1];
		y_4 = right_shu_lines[0][3];
		for (size_t i = 0; i < right_shu_lines.size() - 1; i++)
		{
			if (y_3 <= right_shu_lines[i + 1][1])
			{
				x_3 = right_shu_lines[i + 1][0];
				x_4 = right_shu_lines[i + 1][2];
				y_3 = right_shu_lines[i + 1][1];
				y_4 = right_shu_lines[i + 1][3];
			}
		}
	}
	a = (y_2 - y_1) / (x_2 - x_1);
	b = (x_2 * y_1 - x_1 * y_2) / (x_2 - x_1);
	if ((x_4 - x_3) == 0)  //求左上角坐标值
	{
		right_down_x = x_3;
		right_down_y = a * x_3 + b;
	}
	else
	{
		c = (y_4 - y_3) / (x_4 - x_3);
		d = (x_4 * y_3 - x_3 * y_4) / (x_4 - x_3);
		right_down_x = (d - b) / (a - c);
		right_down_y = (a * d - b * c) / (a - c);
	}
	//==============================================================================
	//在前面进行二值化的时候，由于加宽了外轮廓的宽度造成了一定误差，这里弥补这个误差
	left_up_x = left_up_x + 7;
	left_up_y = left_up_y + 7;
	left_down_x = left_down_x + 7;
	left_down_y = left_down_y - 7;
	right_up_x = right_up_x - 7;
	right_up_y = right_up_y + 7;
	right_down_x = right_down_x - 7;
	right_down_y = right_down_y - 7;

    //points存储外轮廓的四个顶点，rec存储仿射变换到的目标顶点坐标，这里采用860*600的大小，长宽比和身份证真实长宽比一致
	vector<Point2f> points;
	vector<Point2f> rec;
	points.push_back(Point2f(left_up_x, left_up_y));
	points.push_back(Point2f(right_up_x, right_up_y));
	points.push_back(Point2f(left_down_x, left_down_y));
	points.push_back(Point2f(right_down_x, right_down_y));
	rec.push_back(Point2f(0, 0));
	rec.push_back(Point2f(859, 0));
	rec.push_back(Point2f(0, 599));
	rec.push_back(Point2f(859, 599));

	line(img_lines, points[0], points[1], Scalar(255, 0, 0), 1, CV_AA);
	line(img_lines, points[1], points[3], Scalar(255, 0, 0), 1, CV_AA);
	line(img_lines, points[3], points[2], Scalar(255, 0, 0), 1, CV_AA);
	line(img_lines, points[2], points[0], Scalar(255, 0, 0), 1, CV_AA);
	namedWindow("result_lines", 1);
	imshow("result_lines", img_lines);

	line(gray_image, points[0], points[1], Scalar(255, 0, 0), 1, CV_AA);
	line(gray_image, points[1], points[3], Scalar(255, 0, 0), 1, CV_AA);
	line(gray_image, points[3], points[2], Scalar(255, 0, 0), 1, CV_AA);
	line(gray_image, points[2], points[0], Scalar(255, 0, 0), 1, CV_AA);
	namedWindow("result_gray", 1);
	imshow("result_gray", gray_image);

	Mat result_warp;
	Size size(860, 600);
	Mat h = findHomography(points, rec);  //h是仿射变换矩阵
	warpPerspective(gray_image, result_warp, h, size);  //仿射变换
	namedWindow("result_warp", 1);
	imshow("result_warp", result_warp);
	waitKey();

    //将仿射变换后的图片统一调整为1000*700大小，然后在这上面通过选取固定位置切割我们需要的内容
	Mat final;
	resize(result_warp, final, Size(1000, 700));
	imwrite("final.jpg", final);
	Rect rect_1(400, 120, 400, 50);
	Rect rect_2(130, 175, 285, 50);
	Rect rect_3(625, 170, 285, 50);

	Mat part_ID, part_name, part_num;
	final(rect_1).copyTo(part_ID);
	final(rect_2).copyTo(part_name);
	final(rect_3).copyTo(part_num);
	imwrite("part_ID.jpg", part_ID); //切割身份证号部位
	imwrite("part_name.jpg", part_name); //姓名部位
	imwrite("part_num.jpg", part_num); //案编部位
	
	system("tesseract part_ID.jpg output_ID -psm 7");
	system("tesseract part_name.jpg output_name -l chi_sim -psm 7");
	system("tesseract part_num.jpg output_num -psm 7");

	string temp;
	ifstream txt_ID("output_ID.txt");
	getline(txt_ID, temp);
	cout << "身份证号:" << " " << temp << endl;


	CString str_1;
	string str_2;
	CString str_3;
	ifstream txt_name("output_name.txt", ios::in);
	getline(txt_name, str_2);
	str_1 = str_2.c_str();
	UTF8toANSI(str_1);
	//str_3 = str_1.Mid(1);
	cout << "姓名:" << str_1 << endl;


	ifstream txt_num("output_num.txt");
	getline(txt_num, temp);
	cout << "案编号:" << " " << temp << endl;
	while (1);
	return 0;
}
