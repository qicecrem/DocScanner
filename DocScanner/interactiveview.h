#ifndef INTERACTIVEVIEW_H
#define INTERACTIVEVIEW_H

#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsEllipseItem>
#include <QGraphicsPolygonItem>
#include <QWheelEvent>
#include <QImage>
#include <vector>
#include <opencv2/opencv.hpp>

class InteractiveView;

// 控制顶点
class CornerNode : public QGraphicsEllipseItem {
public:
    // 设置固定像素大小，不受缩放影响
    CornerNode(int index, InteractiveView* view, QGraphicsItem* parent = nullptr);

protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;
    // 鼠标形状控制
    void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override;

private:
    int index;
    InteractiveView* parentView;
};

// 交互画布
class InteractiveView : public QGraphicsView {
    Q_OBJECT
public:
    explicit InteractiveView(QWidget *parent = nullptr);
    void setImage(const cv::Mat& image);
    void setCorners(const std::vector<cv::Point2f>& corners);
    std::vector<cv::Point2f> getCorners() const;

public slots:
    void updatePolygon();

protected:
    // 核心：Ctrl+滚轮缩放
    void wheelEvent(QWheelEvent *event) override;

private:
    QGraphicsScene* scene;
    QGraphicsPixmapItem* imageItem;
    //裁剪轮廓
    QGraphicsPolygonItem* polygonItem;
    std::vector<CornerNode*> cornerNodes;

    cv::Mat currentImage;
    QImage matToQImage(const cv::Mat& mat);
};

#endif // INTERACTIVEVIEW_H
