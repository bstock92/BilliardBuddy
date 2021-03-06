#pragma once

#include <opencv2/core/core.hpp>

// Use billbud namespace to prevent ambiguity with cv::FeatureDetector.
namespace billbud
{
	class FeatureDetector
	{
	public:
		FeatureDetector();
		virtual cv::vector<cv::Vec2i> detect(cv::Mat frame, int frameIterator) = 0; // returns a vector of 2D vectors (pixel points)
		//virtual cv::vector<pocket> detect(cv::Mat frame, int type) = 0;
		virtual ~FeatureDetector();
	};
}


