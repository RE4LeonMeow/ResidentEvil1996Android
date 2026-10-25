#pragma once
// "PC assets" tab: migrate a USA or JPN asset tree from a folder or a disc image,
// optionally adding the PS1 ending-credit assets from a PS1 disc image.

#include <QWidget>

class QCheckBox;
class QComboBox;
class QGroupBox;
class QLineEdit;
class QSpinBox;
class RunPanel;

class PcAssetsTab : public QWidget {
    Q_OBJECT
public:
    explicit PcAssetsTab(QWidget* parent = nullptr);

private slots:
    void onSourceKindChanged();
    void onBrowseSource();
    void onBrowsePs1();
    void onBrowseTarget();
    void onBrowseFfmpeg();
    void onDetectFfmpeg();

private:
    void syncEnabled();

    QComboBox* m_sourceKind = nullptr;
    QLineEdit* m_source = nullptr;
    QLineEdit* m_target = nullptr;
    QComboBox* m_version = nullptr;
    QCheckBox* m_convert = nullptr;
    QCheckBox* m_keepAvi = nullptr;
    QLineEdit* m_ffmpeg = nullptr;
    QGroupBox* m_ps1 = nullptr;
    QLineEdit* m_ps1Image = nullptr;
    QCheckBox* m_ps1Credits = nullptr;
    QCheckBox* m_ps1Subs = nullptr;
    QCheckBox* m_ps1Movies = nullptr;
    QCheckBox* m_ps1Replace = nullptr;
    QCheckBox* m_ps1Audio = nullptr;
    QCheckBox* m_ps1AudioSfx = nullptr;
    QCheckBox* m_ps1AudioVoices = nullptr;
    QCheckBox* m_ps1AudioBgm = nullptr;
    QComboBox* m_ps1AudioFormat = nullptr;
    QSpinBox* m_ps1AudioQuality = nullptr;
    RunPanel* m_run = nullptr;
};
