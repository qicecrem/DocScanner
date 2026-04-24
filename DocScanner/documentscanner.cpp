#include "DocumentScanner.h"
#include <iostream>
#include <algorithm>
#include <cmath>

// 在 .cpp 文件中可以使用 using namespace，不会污染其他引用该头文件的类
using namespace std;
using namespace cv;
using namespace cv::dnn;

DocumentScanner::DocumentScanner() {}

// 1. 计算夹角余弦值
double DocumentScanner::getAngle(Point pt1, Point pt2, Point pt0) {
    double dx1 = pt1.x - pt0.x;
    double dy1 = pt1.y - pt0.y;
    double dx2 = pt2.x - pt0.x;
    double dy2 = pt2.y - pt0.y;
    return (dx1 * dx2 + dy1 * dy2) / sqrt((dx1 * dx1 + dy1 * dy1) * (dx2 * dx2 + dy2 * dy2) + 1e-10);
}

// 2. 顶点代数排序算法
vector<Point2f> DocumentScanner::orderPoints(const vector<Point2f>& pts) {
    vector<Point2f> rect(4);
    vector<float> sums(4), diffs(4);
    for (int i = 0; i < 4; i++) {
        sums[i] = pts[i].x + pts[i].y;
        diffs[i] = pts[i].y - pts[i].x;
    }
    rect[0] = pts[distance(sums.begin(), min_element(sums.begin(), sums.end()))]; // 左上
    rect[2] = pts[distance(sums.begin(), max_element(sums.begin(), sums.end()))]; // 右下
    rect[1] = pts[distance(diffs.begin(), min_element(diffs.begin(), diffs.end()))]; // 右上
    rect[3] = pts[distance(diffs.begin(), max_element(diffs.begin(), diffs.end()))]; // 左下
    return rect;
}

// 模式 1：传统视觉算法寻找四个顶点
//双边滤波参数不合理
//Canny 阈值固定
//多通道融合策略低效
vector<Point2f> DocumentScanner::findCornersTraditional(const Mat& image) {
    Mat resized_img;
    float ratio = (float)image.rows / 800.0f;
    int new_width = (int)(image.cols * (800.0f / image.rows));
    resize(image, resized_img, Size(new_width, 800));

    //todo: 不合理
    // 1. 双边滤波 (保留边缘，抹除横纹)
    Mat blurred;
    bilateralFilter(resized_img, blurred, 9, 75, 75);

    // 2. RGB多通道融合边缘检测
    vector<Mat> channels;
    split(blurred, channels);
    Mat merged_edges = Mat::zeros(blurred.size(), CV_8UC1);
    for (int c = 0; c < 3; c++) {
        Mat edges;
        Canny(channels[c], edges, 30, 100);
        bitwise_or(merged_edges, edges, merged_edges);
    }

    Mat kernel = getStructuringElement(MORPH_RECT, Size(3, 3));
    dilate(merged_edges, merged_edges, kernel);

    // 3. 寻找轮廓
    vector<vector<Point>> contours;
    findContours(merged_edges, contours, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);
    sort(contours.begin(), contours.end(), [](const vector<Point>& a, const vector<Point>& b) {
        return contourArea(a) > contourArea(b);
    });

    vector<Point> best_square;
    double max_area = 0;

    // 4. 凸包与动态容差逼近
    for (size_t i = 0; i < min((size_t)10, contours.size()); i++) {
        vector<Point> hull;
        convexHull(contours[i], hull); // 凸包修复缺口
        double peri = arcLength(hull, true);
        vector<Point> approx;

        approxPolyDP(hull, approx, 0.04 * peri, true);

        if (approx.size() == 4 && isContourConvex(approx)) {
            double area = fabs(contourArea(approx));
            if (area > max_area && area > (resized_img.rows * resized_img.cols * 0.05)) {
                // 内角检验
                double maxCosine = 0;
                for (int j = 2; j < 5; j++) {
                    double cosine = fabs(getAngle(approx[j % 4], approx[j - 2], approx[j - 1]));
                    maxCosine = MAX(maxCosine, cosine);
                }
                if (maxCosine < 0.4) { // 角度接近 90 度
                    max_area = area;
                    best_square = approx;
                }
            }
        }
    }

    // 映射回原图坐标
    vector<Point2f> original_corners;
    if (!best_square.empty()) {
        for (const auto& p : best_square) {
            original_corners.push_back(Point2f(p.x * ratio, p.y * ratio));
        }
    }
    return original_corners;
}

// 模式 2：深度学习 AI 寻找四个顶点
vector<Point2f> DocumentScanner::findCornersAI(const Mat& image, Net& net) {
    // 1. 预处理与推理
    Mat blob = blobFromImage(image, 1.0 / 255.0, Size(320, 320), Scalar(0.485, 0.456, 0.406), true, false);
    net.setInput(blob);
    Mat out = net.forward();

    // 2. 提取掩码并转回原图尺寸
    int outDims[] = { 320, 320 };
    Mat mask(2, outDims, CV_32F, out.data);
    Mat resized_mask;
    resize(mask, resized_mask, image.size());

    // 3. 极值归一化与二值化
    double minVal, maxVal;
    minMaxLoc(resized_mask, &minVal, &maxVal);
    resized_mask = (resized_mask - minVal) / (maxVal - minVal);

    Mat binary_mask;
    threshold(resized_mask, binary_mask, 0.5, 255, THRESH_BINARY);
    binary_mask.convertTo(binary_mask, CV_8U);

    // 4. 在干净的 AI 掩码上寻找四边形
    vector<vector<Point>> contours;
    //1.检索模式 外层的轮廓,2.逼近方法
    findContours(binary_mask, contours, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);

    vector<Point2f> corners;
    if (!contours.empty()) {
        auto it = max_element(contours.begin(), contours.end(), [](const vector<Point>& a, const vector<Point>& b) {
            return contourArea(a) < contourArea(b);
        });

        // 动态逼近找4个点
        //true 闭合
        double peri = arcLength(*it, true);
        vector<Point> approx;
        for (double eps = 0.01; eps <= 0.1; eps += 0.01) {
            //3.逼近精度
            approxPolyDP(*it, approx, eps * peri, true);
            if (approx.size() == 4) break;
        }

        if (approx.size() == 4) {
            for (const auto& p : approx) corners.push_back(Point2f(p.x, p.y));
        }
    }
    return corners;
}

// 公共步骤 A：透视变换
Mat DocumentScanner::warpDocument(const Mat& image, const vector<Point2f>& pts) {
    if (pts.size() != 4) return image.clone();

    vector<Point2f> rect = orderPoints(pts);
    Point2f tl = rect[0], tr = rect[1], br = rect[2], bl = rect[3];

    int maxWidth = max((int)sqrt(pow(br.x - bl.x, 2) + pow(br.y - bl.y, 2)),
                       (int)sqrt(pow(tr.x - tl.x, 2) + pow(tr.y - tl.y, 2)));
    int maxHeight = max((int)sqrt(pow(tr.x - br.x, 2) + pow(tr.y - br.y, 2)),
                        (int)sqrt(pow(tl.x - bl.x, 2) + pow(tl.y - bl.y, 2)));

    vector<Point2f> dst = {
        Point2f(0, 0), Point2f(maxWidth - 1, 0),
        Point2f(maxWidth - 1, maxHeight - 1), Point2f(0, maxHeight - 1)
    };

    Mat M = getPerspectiveTransform(rect, dst);
    Mat warped;
    warpPerspective(image, warped, M, Size(maxWidth, maxHeight));
    return warped;
}


Mat DocumentScanner::enhanceDocument(const Mat& warped, ScanMode mode) {
    if (mode == ORIGINAL) {
        return warped.clone();
    }

    Mat gray;
    cvtColor(warped, gray, COLOR_BGR2GRAY);

    if (mode == MAGIC_COLOR) {
        // 1. 转到 Lab 空间进行亮度均衡 (不破坏色彩)
        Mat lab;
        cvtColor(warped, lab, COLOR_BGR2Lab);
        vector<Mat> channels;
        split(lab, channels);

        // 2. 对 L 通道进行 CLAHE 均衡
        //对比度裁剪限制; 分块大小
        Ptr<CLAHE> clahe = createCLAHE(2.0, Size(8, 8));
        clahe->apply(channels[0], channels[0]);

        // 3. 合并并转回 BGR
        merge(channels, lab);
        Mat enhanced;
        cvtColor(lab, enhanced, COLOR_Lab2BGR);

        // 4. 稍微提升饱和度
        Mat hsv;
        cvtColor(enhanced, hsv, COLOR_BGR2HSV);
        vector<Mat> hsvChannels;
        split(hsv, hsvChannels);
        hsvChannels[1] *= 1.2; // 饱和度提升 20%
        merge(hsvChannels, hsv);
        cvtColor(hsv, enhanced, COLOR_HSV2BGR);

        return enhanced;
    }
    else if (mode == GRAYSCALE) {
        // 消除光照不均的灰度增强
        Mat bg_blur, shadow_removed;
        GaussianBlur(gray, bg_blur, Size(51, 51), 0);
        divide(gray, bg_blur, shadow_removed, 255.0);
        return shadow_removed;
    }
    else if (mode == B_W) {
        // 黑白二值化
        Mat bg_blur, shadow_removed, final_bw;
        GaussianBlur(gray, bg_blur, Size(51, 51), 0);
        divide(gray, bg_blur, shadow_removed, 255.0);
        //21 blockSize	局部窗口，计算自适应阈值
        //10 降低阈值
        adaptiveThreshold(shadow_removed, final_bw, 255, ADAPTIVE_THRESH_GAUSSIAN_C, THRESH_BINARY, 21, 10);
        return final_bw;
    }

    return warped;
}

Mat DocumentScanner::dewarpDocument(const Mat& src, float curve_amount) {
    if (abs(curve_amount) < 1.0) return src.clone();

    Mat dst = Mat::zeros(src.size(), src.type());
    Mat map_x(src.size(), CV_32FC1);
    Mat map_y(src.size(), CV_32FC1);

    float h = src.rows;
    float w = src.cols;

    // 算法逻辑：
    // 我们假设书页中间最弯，边缘不弯。使用抛物线方程 y = a*x^2 + b*x + c
    // 简化模型：偏移量 delta_y = offset * sin(pi * x / w)

    for (int y = 0; y < src.rows; y++) {
        for (int x = 0; x < src.cols; x++) {
            // 计算当前列的归一化位置 (0.0 到 1.0)
            float rel_x = (float)x / w;

            // 使用正弦函数模拟书页的隆起 (中间偏移最大，两边为0)
            float offset = curve_amount * sin(M_PI * rel_x);

            // 计算映射坐标：
            // 目标图中的 (x, y) 应该去原图中的 (x, y + offset) 采样
            // 同时为了保持中心对称，对 y 进行缩放补偿
            float target_y = y + offset;

            map_x.at<float>(y, x) = (float)x;
            map_y.at<float>(y, x) = target_y;
        }
    }

    // 最后的 remap 是一次性完成所有像素的重定向，效率极高
    remap(src, dst, map_x, map_y, INTER_LINEAR, BORDER_REPLICATE);

    return dst;
}
