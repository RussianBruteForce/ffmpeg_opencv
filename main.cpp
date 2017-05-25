#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "Classifier.h"
#include "Video.h"

std::vector<char> read_file(const std::string &fname)
{
	std::ifstream file(fname, std::ios::binary | std::ios::ate);
	if (!file.is_open())
		throw std::runtime_error{"can't open " + fname};

	auto size = file.tellg();
	file.seekg(0, std::ios::beg);

	std::cout << "file size: " << std::to_string(size / 1024) << "KiB\n";

	std::vector<char> buffer(size);

	if (file.read(buffer.data(), size))
		return buffer;
	return {};
}

int main(int argc, const char **argv)
{
	if (argc < 2)
		return EXIT_FAILURE;

	av_log_set_level(AV_LOG_DEBUG);

	try {
		auto data = read_file(argv[1]);
		std::cout << "read " << argv[1] << ": "
			  << std::to_string(data.size() / 1024) << "KiB\n";
		size_t f_total{0}, e_total{0};
		Classifier c;
		{
			Video v;
			v.set(data.data(), data.size());

			v.process([&c, &f_total, &e_total](unsigned char *data,
							   int wrap, int xsize,
							   int ysize) {
				cv::Mat frame(ysize, xsize, CV_8UC1, data,
					      wrap);
				auto a = c.classify(frame);
				f_total += a.faces;
				e_total += a.eyes;
			});
			std::cout << "END" << std::endl;
		}
		std::cout << "faces: " << f_total << " eyes: " << e_total
			  << '\n';
	} catch (const std::runtime_error &e) {
		std::cout << "error: " << e.what() << '\n';
	}
}
