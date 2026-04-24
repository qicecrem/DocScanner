#include "interactiveview.h"
#include <QPen>
#include <QBrush>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>

// ==========================================
// CornerNode (控制顶点) 实现
// ==========================================
//(x,y,w,h)
//动态缩放角点大小
CornerNode::CornerNode(int index, InteractiveView* view, QGraphicsItem* parent)
    : QGraphicsEllipseItem(-10, -10, 20, 20, parent), index(index), parentView(view)
{
    //200 透明度
    setBrush(QBrush(QColor(0, 120, 255, 200))); // 稍微加深颜色
    //白色 + 边框宽度 2px
    setPen(QPen(Qt::white, 2));

    // 关键属性：即使视图缩放，该 Item 的视觉大小也保持不变
    setFlag(QGraphicsItem::ItemIgnoresTransformations);

    setFlags(QGraphicsItem::ItemIsMovable |
             QGraphicsItem::ItemIsSelectable |
             QGraphicsItem::ItemSendsGeometryChanges);
    //悬停检测
    setAcceptHoverEvents(true);
    setZValue(2);
}

QVariant CornerNode::itemChange(GraphicsItemChange change, const QVariant &value) {
    if (change == ItemPositionHasChanged && parentView) {
        parentView->updatePolygon();
    }
    return QGraphicsEllipseItem::itemChange(change, value);
}

void CornerNode::hoverEnterEvent(QGraphicsSceneHoverEvent *event) {
    setCursor(Qt::SizeAllCursor);
    QGraphicsEllipseItem::hoverEnterEvent(event);
}

// ==========================================
// InteractiveView (交互画布) 实现
// ==========================================
InteractiveView::InteractiveView(QWidget *parent) : QGraphicsView(parent) {
    scene = new QGraphicsScene(this);
    setScene(scene);
    //抗锯齿
    setRenderHint(QPainter::Antialiasing);
    setRenderHint(QPainter::SmoothPixmapTransform); // 让图片缩放更平滑

    // 允许鼠标拖拽平移画布（非拖拽点时）
    setDragMode(QGraphicsView::ScrollHandDrag);

    // 设置缩放锚点：以鼠标下方的点为中心缩放
    // 缩放/变换锚点
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    // 窗口 resize 锚点
    setResizeAnchor(QGraphicsView::AnchorUnderMouse);

    imageItem = new QGraphicsPixmapItem();
    scene->addItem(imageItem);
    //polygon-多边形 solidline-实线
    polygonItem = new QGraphicsPolygonItem();
    polygonItem->setPen(QPen(QColor(0, 255, 0, 180), 3, Qt::SolidLine));
    polygonItem->setZValue(1);
    scene->addItem(polygonItem);

    // 隐藏滚动条让界面更清爽
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
}

void InteractiveView::setImage(const cv::Mat& image) {
    if (image.empty()) return;
    currentImage = image.clone();

    QImage qimg = matToQImage(currentImage);
    imageItem->setPixmap(QPixmap::fromImage(qimg));
    scene->setSceneRect(qimg.rect());

    // 初始显示时适应窗口大小
    fitInView(scene->sceneRect(), Qt::KeepAspectRatio);
}

void InteractiveView::setCorners(const std::vector<cv::Point2f>& corners) {
    for (auto node : cornerNodes) {
        scene->removeItem(node);
        delete node;
    }
    cornerNodes.clear();

    std::vector<cv::Point2f> pts = corners;
    if (pts.size() != 4) {
        float w = currentImage.cols;
        float h = currentImage.rows;
        pts = { cv::Point2f(w*0.1, h*0.1), cv::Point2f(w*0.9, h*0.1),
               cv::Point2f(w*0.9, h*0.9), cv::Point2f(w*0.1, h*0.9) };
    }

    for (int i = 0; i < 4; ++i) {
        CornerNode* node = new CornerNode(i, this);
        node->setPos(pts[i].x, pts[i].y);
        scene->addItem(node);
        cornerNodes.push_back(node);
    }
    updatePolygon();
}

void InteractiveView::updatePolygon() {
    if (cornerNodes.size() != 4) return;
    QPolygonF poly;
    for (auto node : cornerNodes) poly << node->pos();
    polygonItem->setPolygon(poly);
}

std::vector<cv::Point2f> InteractiveView::getCorners() const {
    std::vector<cv::Point2f> pts;
    for (auto node : cornerNodes) {
        pts.push_back(cv::Point2f(node->x(), node->y()));
    }
    return pts;
}

// 核心功能：Ctrl + 滚轮缩放
void InteractiveView::wheelEvent(QWheelEvent *event) {
    if (event->modifiers() & Qt::ControlModifier) {
        const double factor = 1.15;
        if (event->angleDelta().y() > 0) {
            scale(factor, factor); // 放大
        } else {
            scale(1.0 / factor, 1.0 / factor); // 缩小
        }
    } else {
        // 非 Ctrl 键时保持默认滚动行为
        QGraphicsView::wheelEvent(event);
    }
}

QImage InteractiveView::matToQImage(const cv::Mat& mat) {
    if (mat.type() == CV_8UC1) {
        QImage image(mat.cols, mat.rows, QImage::Format_Indexed8);
        image.setColorCount(256);
        for(int i = 0; i < 256; i++) image.setColor(i, qRgb(i, i, i));
        uchar *pSrc = mat.data;
        for(int row = 0; row < mat.rows; row ++) {
            uchar *pDest = image.scanLine(row);
            memcpy(pDest, pSrc, mat.cols);
            pSrc += mat.step;
        }
        return image;
    } else if (mat.type() == CV_8UC3) {
        cv::Mat rgbMat;
        cv::cvtColor(mat, rgbMat, cv::COLOR_BGR2RGB);
        QImage image(rgbMat.data, rgbMat.cols, rgbMat.rows, rgbMat.step, QImage::Format_RGB888);
        return image.copy();
    }
    return QImage();
}
