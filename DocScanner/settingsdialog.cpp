#include "settingsdialog.h"
#include "ui_settingsdialog.h"
#include "configmanager.h"
#include <QFileDialog>
#include <QCoreApplication>

SettingsDialog::SettingsDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::SettingsDialog)
{
    ui->setupUi(this);
    setWindowTitle("⚙️ 系统配置中心");
    loadSettings();
}

SettingsDialog::~SettingsDialog() {
    delete ui;
}

void SettingsDialog::loadSettings() {
    // 提供友好的默认值
    QString defaultModelPath = QCoreApplication::applicationDirPath() + "/model/u2netp.onnx";
    QString defaultTessPath = QCoreApplication::applicationDirPath() + "/tessdata";

    ui->editModelPath->setText(ConfigManager::instance().getString("ai_model_path", defaultModelPath));
    ui->editTessPath->setText(ConfigManager::instance().getString("tessdata_path", defaultTessPath)); // 新增
    ui->comboDefaultFilter->setCurrentIndex(ConfigManager::instance().getInt("default_filter", 3));
    ui->spinDefaultCurve->setValue(ConfigManager::instance().getInt("default_curve", 0));
}

void SettingsDialog::on_btnBrowseModel_clicked() {
    QString path = QFileDialog::getOpenFileName(this, "选择 ONNX 模型", "", "ONNX Models (*.onnx)");
    if (!path.isEmpty()) {
        ui->editModelPath->setText(path);
    }
}

// 选择 OCR 语言包文件夹
void SettingsDialog::on_btnBrowseTess_clicked() {
    QString dir = QFileDialog::getExistingDirectory(this, "选择 tessdata 文件夹 (包含 chi_sim.traineddata)");
    if (!dir.isEmpty()) {
        ui->editTessPath->setText(dir);
    }
}

void SettingsDialog::on_buttonBox_accepted() {
    ConfigManager::instance().setValue("ai_model_path", ui->editModelPath->text());
    ConfigManager::instance().setValue("tessdata_path", ui->editTessPath->text());
    ConfigManager::instance().setValue("default_filter", ui->comboDefaultFilter->currentIndex());
    ConfigManager::instance().setValue("default_curve", ui->spinDefaultCurve->value());
    accept();
}
