#pragma once

#include <cstdint>

#include <opencv2/objdetect/objdetect.hpp>

namespace cv
{
class Mat;
}

class Classifier
{
	static cv::String face_cascade_name;
	static cv::String eyes_cascade_name;

	static cv::Size face_min_size;
	static cv::Size eye_min_size;

      public:
	struct answer {
		uint_fast16_t faces;
		uint_fast16_t eyes;
	};

	Classifier();

	answer classify(const cv::Mat &frame);

      private:
	cv::CascadeClassifier face_cascade;
	cv::CascadeClassifier eyes_cascade;
};
