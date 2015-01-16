#pragma once

#include "FeatureDetector.h"

#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>

class PoolTableDetector :
	public virtual billbud::FeatureDetector
{
private:
	void detectTableWithColourSegmentation(cv::Mat& frame);
	void detectPocketsWithColourSegmentation(cv::Mat& frame);
	void detectTableEdge(cv::Mat& frame, cv::Mat& tableMask);
	void regHoughLines(cv::Mat& view, cv::Mat& houghMap, int threshold);
	void probHoughLines(cv::Mat& view, cv::Mat& houghMap, int threshold, int minLineLength, int maxLineGap);
	cv::Mat colourSegmentation(cv::Mat frame, int open_size, int close_size, int iLowH, int iLowS, int iLowV,
		int iHighH, int iHighS, int iHighV);
	cv::vector<cv::Vec2i> pockets;

public:
	PoolTableDetector();
	cv::vector<cv::Vec2i> detect(cv::Mat frame);
	~PoolTableDetector();
};

