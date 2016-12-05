#include "Classifier.h"

#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>

cv::String Classifier::face_cascade_name = "lbpcascade_frontalface.xml";
cv::String Classifier::eyes_cascade_name =
    "haarcascade_eye_tree_eyeglasses.xml";

cv::Size Classifier::face_min_size = cv::Size(30, 30);
cv::Size Classifier::eye_min_size = cv::Size(30, 15);

Classifier::Classifier()
{
	if (!face_cascade.load(face_cascade_name))
		throw std::runtime_error{"can't open " + face_cascade_name};

	if (!eyes_cascade.load(eyes_cascade_name))
		throw std::runtime_error{"can't open " + eyes_cascade_name};
}

Classifier::answer Classifier::classify(const cv::Mat &frame)
{
	std::vector<cv::Rect> faces;
	face_cascade.detectMultiScale(
	    frame, faces, 1.1, 2, 0 | cv::CASCADE_SCALE_IMAGE, face_min_size);

	answer ret{0, 0};
	for (auto &face : faces) {
		cv::Mat faceROI = frame(face);
		std::vector<cv::Rect> eyes;
		eyes_cascade.detectMultiScale(faceROI, eyes, 1.1, 2,
					      0 | cv::CASCADE_SCALE_IMAGE,
					      eye_min_size);

		ret.eyes += eyes.size();
	}

	ret.faces = faces.size();
	return ret;
}
