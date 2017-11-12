# driving-licence-recognition
driving licence recognition using Tesseract-OCR 4.0
image_preprocess.cpp程序功能是：利用opencv自带函数，对采集到的驾驶证图片提取外边缘轮廓，并通过仿射变换进行校正，然后调整为统一大小，选取身份证号等对应内容的位置进行切割，之后再用Tesseract-OCR对切割出来的内容进行识别
