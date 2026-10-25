#pragma once
// "Director's Cut" tab: build the DC overlay from a PS1 disc image.

#include <QWidget>

class QCheckBox;
class QComboBox;
class QLineEdit;
class RunPanel;

class DcAssetsTab : public QWidget {
    Q_OBJECT
public:
    explicit DcAssetsTab(QWidget* parent = nullptr);

private slots:
    void onBrowseImage();
    void onBrowseTarget();
    void onBrowseFfmpeg();
    void onDetectFfmpeg();

private:
    void syncEnabled();

    QLineEdit* m_image = nullptr;
    QLineEdit* m_target = nullptr;
    QComboBox* m_base = nullptr;
    QCheckBox* m_convert = nullptr;
    QCheckBox* m_subs = nullptr;
    QCheckBox* m_verify = nullptr;
    QLineEdit* m_ffmpeg = nullptr;
    RunPanel* m_run = nullptr;
};
