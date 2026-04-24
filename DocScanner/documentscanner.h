#ifndef DOCUMENT_SCANNER_H
#define DOCUMENT_SCANNER_H

#include <opencv2/opencv.hpp>
#include <opencv2/dnn.hpp>
#include <vector>

class DocumentScanner {
public:
    enum ScanMode {
        ORIGINAL = 0,    // 原图矫正
        MAGIC_COLOR = 1, // 增强显色
        GRAYSCALE = 2,   // 省墨模式
        B_W = 3          // 黑白二值化
    };

    DocumentScanner();

    std::vector<cv::Point2f> findCornersTraditional(const cv::Mat& image);
    std::vector<cv::Point2f> findCornersAI(const cv::Mat& image, cv::dnn::Net& net);
    cv::Mat warpDocument(const cv::Mat& image, const std::vector<cv::Point2f>& pts);
    cv::Mat enhanceDocument(const cv::Mat& warped, ScanMode mode);
    cv::Mat dewarpDocument(const cv::Mat& src, float curve_amount);

private:
    double getAngle(cv::Point pt1, cv::Point pt2, cv::Point pt0);
    std::vector<cv::Point2f> orderPoints(const std::vector<cv::Point2f>& pts);
};

#endif // DOCUMENT_SCANNER_H
