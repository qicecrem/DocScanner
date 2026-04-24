#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QtConcurrent>
#include <tesseract/baseapi.h>
#include <leptonica/allheaders.h>
#include <QPdfWriter>
#include <QPainter>
#include <QListWidgetItem>
#include <QClipboard>
#include "configmanager.h"
#include "settingsdialog.h"
#include <QTextEdit>
#include <QVBoxLayout>
#include <QDialog>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    ui->resultView->setAcceptDrops(false);

    watcher = new QFutureWatcher<void>(this);
    connect(watcher, &QFutureWatcher<void>::finished, this, &MainWindow::on_scanFinished);

    cropWatcher = new QFutureWatcher<void>(this);
    connect(cropWatcher, &QFutureWatcher<void>::finished, this, &MainWindow::on_cropFinished);

    ocrWatcher = new QFutureWatcher<QString>(this);
    connect(ocrWatcher, &QFutureWatcher<QString>::finished, this, &MainWindow::on_ocrFinished);

    connect(ui->sliderDewarp, &QSlider::valueChanged, this, &MainWindow::updateResultDisplay);
    connect(ui->thumbnailList, &QListWidget::currentItemChanged, this, &MainWindow::on_thumbnailList_currentItemChanged);

    loadAIModel();
}

MainWindow::~MainWindow() { delete ui; }

void MainWindow::setUIEnabled(bool enabled) {
    ui->btnLoadImage->setEnabled(enabled);
    ui->btnScanAI->setEnabled(enabled);
    ui->btnScanTraditional->setEnabled(enabled);
    ui->btnCrop->setEnabled(enabled);
    ui->btnOCR->setEnabled(enabled);
    ui->btnExportPDF->setEnabled(enabled);
    ui->btnRotateLeft->setEnabled(enabled);
    ui->btnRotateRight->setEnabled(enabled);
    ui->thumbnailList->setEnabled(enabled);
    if (!enabled) setCursor(Qt::WaitCursor);
    else unsetCursor();
}

void MainWindow::loadAIModel() {
    QString defaultPath = QCoreApplication::applicationDirPath() + "/model/u2netp.onnx";
    QString modelPath = ConfigManager::instance().getString("ai_model_path", defaultPath);
    try {
        ai_net = cv::dnn::readNetFromONNX(modelPath.toStdString());
        ui->statusbar->showMessage("系统就绪：AI 模型加载成功");
    } catch (cv::Exception& e) {
        ui->statusbar->showMessage("警告：AI 模型加载失败，请检查配置中心！");
    }
}

QIcon MainWindow::generateThumbnail(const cv::Mat& mat) {
    QImage img = convertMatToQImage(mat);
    //QPixmap：用于界面显示
    //KeepAspectRatio：保持宽高比
    //SmoothTransformation：平滑缩放算法
    return QIcon(QPixmap::fromImage(img).scaled(100, 140, Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void MainWindow::on_btnLoadImage_clicked() {
    QStringList fileNames = QFileDialog::getOpenFileNames(this, "选择文档图片 (支持多选)", "", "Images (*.png *.jpg *.jpeg *.bmp)");
    if (fileNames.isEmpty()) return;

    ui->statusbar->showMessage("正在导入图片，请稍候...");
    int defaultFilter = ConfigManager::instance().getInt("default_filter", 3);
    int defaultCurve = ConfigManager::instance().getInt("default_curve", 0);

    for (const QString& fileName : fileNames) {
        //兼容中文,C++ 标准字符串
        cv::Mat img = cv::imread(fileName.toLocal8Bit().toStdString());
        if (img.empty()) continue;

        PageData page;
        page.id = QUuid::createUuid().toString();
        page.originalImage = img;
        page.filterIndex = defaultFilter;
        page.dewarpAmount = defaultCurve;
        documentManager.insert(page.id, page);

        QListWidgetItem *item = new QListWidgetItem(ui->thumbnailList);
        item->setIcon(generateThumbnail(img));
        item->setText(QString("待处理"));
        item->setData(Qt::UserRole, page.id);
    }

    if (ui->thumbnailList->count() > 0 && !ui->thumbnailList->currentItem()) {
        ui->thumbnailList->setCurrentRow(0);
    }

    ui->statusbar->showMessage(QString("成功导入 %1 张图片").arg(fileNames.size()));
    ui->btnExportPDF->setEnabled(true);
}

void MainWindow::saveCurrentPageConfig() {
    if (currentPageId.isEmpty() || !documentManager.contains(currentPageId)) return;
    PageData& data = documentManager[currentPageId];
    data.corners = ui->graphicsView->getCorners();
    data.filterIndex = ui->comboFilter->currentIndex();
    data.dewarpAmount = ui->sliderDewarp->value();
}

void MainWindow::loadPageToUI(const QString& id) {
    if (!documentManager.contains(id)) return;
    currentPageId = id;
    PageData& data = documentManager[id];

    ui->comboFilter->blockSignals(true);
    ui->sliderDewarp->blockSignals(true);

    ui->graphicsView->setImage(data.originalImage);
    ui->graphicsView->setCorners(data.corners);
    ui->comboFilter->setCurrentIndex(data.filterIndex);
    ui->sliderDewarp->setValue(data.dewarpAmount);
    ui->resultView->setImage(data.resultImage);

    ui->comboFilter->blockSignals(false);
    ui->sliderDewarp->blockSignals(false);

    ui->btnSave->setEnabled(data.isProcessed);
    ui->btnOCR->setEnabled(data.isProcessed);
}

void MainWindow::on_thumbnailList_currentItemChanged(QListWidgetItem *current, QListWidgetItem *previous) {
    if (previous) saveCurrentPageConfig();
    if (current) loadPageToUI(current->data(Qt::UserRole).toString());
}

// ==========================================
// 图片旋转功能
// ==========================================
void MainWindow::on_btnRotateLeft_clicked() {
    if (currentPageId.isEmpty() || !documentManager.contains(currentPageId)) return;
    saveCurrentPageConfig();
    PageData& data = documentManager[currentPageId];

    // 执行逆时针 90 度旋转
    cv::rotate(data.originalImage, data.originalImage, cv::ROTATE_90_COUNTERCLOCKWISE);

    // 因为图像旋转了，之前的锚点和裁剪结果全部作废，需要清空
    data.corners.clear();
    data.isProcessed = false;
    data.warpedImage = cv::Mat();
    data.resultImage = cv::Mat();

    loadPageToUI(currentPageId); // 重新加载到画面
    if(ui->thumbnailList->currentItem()) {
        ui->thumbnailList->currentItem()->setIcon(generateThumbnail(data.originalImage));
        ui->thumbnailList->currentItem()->setText("已旋转");
    }
}

void MainWindow::on_btnRotateRight_clicked() {
    if (currentPageId.isEmpty() || !documentManager.contains(currentPageId)) return;
    saveCurrentPageConfig();
    PageData& data = documentManager[currentPageId];

    // 执行顺时针 90 度旋转
    cv::rotate(data.originalImage, data.originalImage, cv::ROTATE_90_CLOCKWISE);

    data.corners.clear();
    data.isProcessed = false;
    data.warpedImage = cv::Mat();
    data.resultImage = cv::Mat();

    loadPageToUI(currentPageId);
    if(ui->thumbnailList->currentItem()) {
        ui->thumbnailList->currentItem()->setIcon(generateThumbnail(data.originalImage));
        ui->thumbnailList->currentItem()->setText("已旋转");
    }
}

//为什么不像traditional 要用线程
void MainWindow::on_btnScanAI_clicked() {
    if (ai_net.empty() || documentManager.isEmpty()) return;
    saveCurrentPageConfig();
    setUIEnabled(false);
    ui->statusbar->showMessage("正在执行后台批量 AI 分析...");

    QList<QListWidgetItem*> selectedItems = ui->thumbnailList->selectedItems();
    if (selectedItems.isEmpty()) {
        for (int i = 0; i < ui->thumbnailList->count(); ++i) selectedItems.append(ui->thumbnailList->item(i));
    }
    QStringList targetIds;
    for ( auto item : selectedItems) targetIds.append(item->data(Qt::UserRole).toString());

    QFuture<void> future = QtConcurrent::run([this, targetIds]() {
        for (const QString& id : targetIds) {
            if (documentManager.contains(id)) {
                cv::Mat img = documentManager[id].originalImage;
                documentManager[id].corners = scanner.findCornersAI(img, ai_net);
            }
        }
    });
    watcher->setFuture(future);
}

void MainWindow::on_scanFinished() {
    setUIEnabled(true);
    if (!currentPageId.isEmpty() && documentManager.contains(currentPageId)) {
        ui->graphicsView->setCorners(documentManager[currentPageId].corners);
    }
    ui->statusbar->showMessage("智能识别完成！");
}

void MainWindow::on_btnScanTraditional_clicked() {
    if (currentPageId.isEmpty()) return;
    ui->graphicsView->setCorners(scanner.findCornersTraditional(documentManager[currentPageId].originalImage));
}

// ==========================================
// 批量裁剪引擎 (核心升级)
// ==========================================
void MainWindow::on_btnCrop_clicked() {
    if (documentManager.isEmpty()) return;
    saveCurrentPageConfig();
    setUIEnabled(false);
    ui->statusbar->showMessage("正在执行后台批量裁剪与滤镜渲染，请稍候...");

    // 获取需要处理的目标 (如果没选中特定的某几张，默认处理列表里的全部)
    QList<QListWidgetItem*> selectedItems = ui->thumbnailList->selectedItems();
    if (selectedItems.isEmpty()) {
        for (int i = 0; i < ui->thumbnailList->count(); ++i) selectedItems.append(ui->thumbnailList->item(i));
    }

    QStringList targetIds;
    for (auto item : selectedItems) targetIds.append(item->data(Qt::UserRole).toString());

    // 放入后台多线程运行，防止 UI 卡死
    QFuture<void> future = QtConcurrent::run([this, targetIds]() {
        for (const QString& id : targetIds) {
            if (documentManager.contains(id)) {
                PageData& data = documentManager[id];
                // 只有成功识别出4个点才进行透视裁剪
                if (data.corners.size() == 4) {
                    data.warpedImage = scanner.warpDocument(data.originalImage, data.corners);
                    cv::Mat dewarped = scanner.dewarpDocument(data.warpedImage, (float)data.dewarpAmount);
                    DocumentScanner::ScanMode mode = static_cast<DocumentScanner::ScanMode>(data.filterIndex);
                    data.resultImage = scanner.enhanceDocument(dewarped, mode);
                    data.isProcessed = true;
                }
            }
        }
    });
    cropWatcher->setFuture(future);
}

// 批量裁剪完成后的 UI 刷新
void MainWindow::on_cropFinished() {
    setUIEnabled(true);

    // 遍历列表，刷新所有处理过的缩略图
    for (int i = 0; i < ui->thumbnailList->count(); ++i) {
        QListWidgetItem* item = ui->thumbnailList->item(i);
        QString id = item->data(Qt::UserRole).toString();
        if (documentManager.contains(id) && documentManager[id].isProcessed) {
            item->setIcon(generateThumbnail(documentManager[id].resultImage));
            item->setText("✅ 已裁剪");
        }
    }

    // 刷新当前画面
    loadPageToUI(currentPageId);
    ui->statusbar->showMessage("✅ 裁剪与滤镜处理完成！");
}

void MainWindow::updateResultDisplay() {
    if (currentPageId.isEmpty() || !documentManager.contains(currentPageId)) return;
    PageData& data = documentManager[currentPageId];
    if (data.warpedImage.empty()) return;

    float amount = (float)ui->sliderDewarp->value();
    cv::Mat dewarped = scanner.dewarpDocument(data.warpedImage, amount);
    int index = ui->comboFilter->currentIndex();
    DocumentScanner::ScanMode mode = static_cast<DocumentScanner::ScanMode>(index);

    data.resultImage = scanner.enhanceDocument(dewarped, mode);
    ui->resultView->setImage(data.resultImage);
}

void MainWindow::on_comboFilter_currentIndexChanged(int index) { Q_UNUSED(index); updateResultDisplay(); }

void MainWindow::on_btnExportPDF_clicked() {
    if (ui->thumbnailList->count() == 0) return;
    QList<QListWidgetItem*> exportItems = ui->thumbnailList->selectedItems();
    if (exportItems.isEmpty()) {
        for (int i = 0; i < ui->thumbnailList->count(); ++i) exportItems.append(ui->thumbnailList->item(i));
    }
    std::sort(exportItems.begin(), exportItems.end(), [this](QListWidgetItem* a, QListWidgetItem* b) {
        return ui->thumbnailList->row(a) < ui->thumbnailList->row(b);
    });

    QString savePath = QFileDialog::getSaveFileName(this, "导出顺序 PDF", "MultiPageDocument.pdf", "PDF Files (*.pdf)");
    if (savePath.isEmpty()) return;

    setCursor(Qt::WaitCursor);
    QPdfWriter pdfWriter(savePath);
    pdfWriter.setPageSize(QPageSize(QPageSize::A4));
    pdfWriter.setResolution(300);

    QPainter painter(&pdfWriter);
    bool isFirstPage = true;

    for (auto item : exportItems) {
        QString id = item->data(Qt::UserRole).toString();
        PageData& data = documentManager[id];
        cv::Mat targetMat = data.isProcessed ? data.resultImage : data.originalImage;
        QImage img = convertMatToQImage(targetMat);
        if (img.isNull()) continue;

        if (!isFirstPage) pdfWriter.newPage();
        isFirstPage = false;
        QRect viewport = painter.viewport();
        QImage scaledImg = img.scaled(viewport.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
        int x = (viewport.width() - scaledImg.width()) / 2;
        int y = (viewport.height() - scaledImg.height()) / 2;
        painter.drawImage(x, y, scaledImg);
    }
    painter.end();
    unsetCursor();
    QMessageBox::information(this, "导出成功", QString("成功导出 %1 页文档到 PDF！").arg(exportItems.size()));
}

void MainWindow::on_btnSave_clicked() {
    if (currentPageId.isEmpty() || !documentManager.contains(currentPageId)) return;
    cv::Mat res = documentManager[currentPageId].resultImage;
    if (res.empty()) return;
    QString savePath = QFileDialog::getSaveFileName(this, "保存图片", "ScanResult.jpg", "Images (*.jpg *.png)");
    if (!savePath.isEmpty()) {
        cv::imwrite(savePath.toLocal8Bit().toStdString(), res);
        ui->statusbar->showMessage("图片保存成功: " + savePath);
    }
}

void MainWindow::on_btnOCR_clicked() {
    if (currentPageId.isEmpty() || !documentManager.contains(currentPageId)) return;
    cv::Mat targetMat = documentManager[currentPageId].resultImage;
    if (targetMat.empty()) {
        QMessageBox::warning(this, "提示", "请先执行裁剪并生成结果！"); return;
    }
    setUIEnabled(false);
    ui->statusbar->showMessage("正在进行 OCR，请稍候...");
    cv::Mat imageForOCR = targetMat.clone();
    QString defaultTess = QCoreApplication::applicationDirPath() + "/tessdata";
    QString tessPath = ConfigManager::instance().getString("tessdata_path", defaultTess);

    QFuture<QString> future = QtConcurrent::run([imageForOCR, tessPath]() -> QString {
        qputenv("TESSDATA_PREFIX", tessPath.toLocal8Bit());
        tesseract::TessBaseAPI *api = new tesseract::TessBaseAPI();
        if (api->Init(nullptr, "chi_sim+eng")) { delete api; return "OCR 初始化失败！"; }
        cv::Mat gray;
        if (imageForOCR.channels() == 3) cv::cvtColor(imageForOCR, gray, cv::COLOR_BGR2GRAY);
        else gray = imageForOCR;
        api->SetImage((uchar*)gray.data, gray.cols, gray.rows, 1, gray.step);
        char* outText = api->GetUTF8Text();
        QString result = QString::fromUtf8(outText);
        api->End(); delete[] outText; delete api;
        return result;
    });
    ocrWatcher->setFuture(future);
}

void MainWindow::on_ocrFinished() {
    setUIEnabled(true);
    QString text = ocrWatcher->result();
    ui->statusbar->showMessage("OCR 识别完成！");
    QDialog *ocrDialog = new QDialog(this);
    ocrDialog->setWindowTitle("📝 文本提取结果");
    ocrDialog->resize(600, 450);
    ocrDialog->setStyleSheet("QDialog { background-color: #1e1e1e; } QTextEdit { background-color: #252525; color: #e0e0e0; border: 1px solid #444; border-radius: 6px; padding: 12px; font-size: 14px; font-family: 'Microsoft YaHei'; line-height: 1.5; } QPushButton { background-color: #0e639c; color: white; border: none; border-radius: 5px; padding: 10px; font-size: 14px; font-weight: bold; } QPushButton:hover { background-color: #1177bb; }");

    //等价
    //QVBoxLayout *layout = new QVBoxLayout;
    //ocrDialog->setLayout(layout);

    QVBoxLayout *layout = new QVBoxLayout(ocrDialog);
    layout->setSpacing(15);
    QTextEdit *textEdit = new QTextEdit(ocrDialog);
    textEdit->setPlainText(text);
    layout->addWidget(textEdit);
    QPushButton *btnCopy = new QPushButton("📋 复制全部文本到剪贴板", ocrDialog);
    layout->addWidget(btnCopy);
    connect(btnCopy, &QPushButton::clicked, [text, ocrDialog]() {
        QApplication::clipboard()->setText(text);
        QMessageBox::information(ocrDialog, "复制成功", "已复制到剪贴板！");
    });
    ocrDialog->exec();
    delete ocrDialog;
}

void MainWindow::on_btnSettings_clicked() {
    SettingsDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        loadAIModel();
        ui->comboFilter->setCurrentIndex(ConfigManager::instance().getInt("default_filter", 3));
        ui->sliderDewarp->setValue(ConfigManager::instance().getInt("default_curve", 0));
        updateResultDisplay();
    }
}

QImage MainWindow::convertMatToQImage(const cv::Mat& mat) {
    if (mat.type() == CV_8UC1) {
        return QImage(mat.data, mat.cols, mat.rows, mat.step, QImage::Format_Grayscale8).copy();
    } else if (mat.type() == CV_8UC3) {
        cv::Mat rgbMat;
        cv::cvtColor(mat, rgbMat, cv::COLOR_BGR2RGB);
        return QImage(rgbMat.data, rgbMat.cols, rgbMat.rows, rgbMat.step, QImage::Format_RGB888).copy();
    }
    return QImage();
}
