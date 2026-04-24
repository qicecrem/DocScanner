#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QFutureWatcher>
#include <vector>
#include <opencv2/opencv.hpp>
#include <QString>
#include <QHash>
#include <QUuid>
#include "documentscanner.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

// 定义单页文档的数据结构
struct PageData {
    QString id;                             // 唯一ID
    cv::Mat originalImage;                  // 原始图片
    std::vector<cv::Point2f> corners;       // 手动/AI调整的四个锚点
    cv::Mat warpedImage;                    // 裁剪后的缓存图片
    cv::Mat resultImage;                    // 经过滤镜处理后的最终图片
    int filterIndex = 3;                    // 当前页的滤镜状态 (默认黑白)
    int dewarpAmount = 0;                   // 当前页的曲面平展强度
    bool isProcessed = false;               // 是否已经执行过裁剪
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void on_btnLoadImage_clicked();
    void on_btnScanAI_clicked();
    void on_btnScanTraditional_clicked();

    // ======== 旋转与批量裁剪 ========
    void on_btnRotateLeft_clicked();
    void on_btnRotateRight_clicked();
    void on_btnCrop_clicked();
    void on_cropFinished();

    void on_scanFinished();
    void on_btnSave_clicked();
    void on_comboFilter_currentIndexChanged(int index);

    void on_btnOCR_clicked();
    void on_ocrFinished();

    void on_btnExportPDF_clicked();
    void on_btnSettings_clicked();
    void on_thumbnailList_currentItemChanged(class QListWidgetItem *current, class QListWidgetItem *previous);

private:
    Ui::MainWindow *ui;

    QHash<QString, PageData> documentManager;
    QString currentPageId;

    DocumentScanner scanner;
    cv::dnn::Net ai_net;

    QFutureWatcher<void> *watcher;            // 批量AI处理的监听器
    QFutureWatcher<void> *cropWatcher;        // 批量裁剪处理的监听器
    QFutureWatcher<QString> *ocrWatcher;      // OCR 监听器

    void updateResultDisplay();
    void setUIEnabled(bool enabled);
    void loadAIModel();
    void saveCurrentPageConfig();
    void loadPageToUI(const QString& id);
    QImage convertMatToQImage(const cv::Mat& mat);
    QIcon generateThumbnail(const cv::Mat& mat);
};

#endif // MAINWINDOW_H
