#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>

namespace Ui { class SettingsDialog; }

class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget *parent = nullptr);
    ~SettingsDialog();

private slots:
    void on_btnBrowseModel_clicked();
    void on_btnBrowseTess_clicked();
    void on_buttonBox_accepted();

private:
    Ui::SettingsDialog *ui;
    void loadSettings();
};

#endif // SETTINGSDIALOG_H
