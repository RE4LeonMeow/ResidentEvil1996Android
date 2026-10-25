#include "PcAssetsTab.h"

#include "RunPanel.h"
#include "Worker.h"
#include "core/Migrate.h"

#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QStandardPaths>
#include <QVBoxLayout>

namespace {

QWidget* pathRow(QLineEdit* edit, QPushButton* browse) {
    auto* w = new QWidget();
    auto* l = new QHBoxLayout(w);
    l->setContentsMargins(0, 0, 0, 0);
    l->addWidget(edit, 1);
    l->addWidget(browse);
    return w;
}

// A word-wrapped QLabel reports a minimum height for a guessed width, not the
// one it ends up with, so the window's minimum size can come out too short and
// the layout crops the text. Pin the minimum height to the wrapped height at
// the label's real width so the window grows to fit it instead.
class WrappedNote : public QLabel {
public:
    WrappedNote(const QString& text, QWidget* parent) : QLabel(text, parent) {
        setWordWrap(true);
    }

protected:
    void resizeEvent(QResizeEvent* e) override {
        QLabel::resizeEvent(e);
        const int h = heightForWidth(width());
        if (h > 0 && h != minimumHeight()) setMinimumHeight(h);
    }
};

}  // namespace

PcAssetsTab::PcAssetsTab(QWidget* parent) : QWidget(parent) {
    m_sourceKind = new QComboBox(this);
    m_sourceKind->addItem(tr("Folder (already extracted)"), false);
    m_sourceKind->addItem(tr("Disc image (.iso / .bin / .cue)"), true);

    m_source = new QLineEdit(this);
    auto* browseSource = new QPushButton(tr("Browse..."), this);

    m_target = new QLineEdit(this);
    m_target->setText(QCoreApplication::applicationDirPath());
    auto* browseTarget = new QPushButton(tr("Browse..."), this);

    m_version = new QComboBox(this);
    m_version->addItem(tr("USA (North American / GOG)"), (int)re1::AssetVersion::USA);
    m_version->addItem(tr("Japanese (Biohazard) / GOG"), (int)re1::AssetVersion::JPN);

    m_convert = new QCheckBox(tr("Convert movies (AVI -> MP4)"), this);
    m_keepAvi = new QCheckBox(tr("Keep the original .avi next to the .mp4"), this);
    m_keepAvi->setChecked(true);

    m_ffmpeg = new QLineEdit(this);
    m_ffmpeg->setText(QStringLiteral("ffmpeg"));
    auto* browseFfmpeg = new QPushButton(tr("Browse..."), this);
    auto* detectFfmpeg = new QPushButton(tr("Detect"), this);
    auto* ffmpegRow = new QWidget();
    {
        auto* l = new QHBoxLayout(ffmpegRow);
        l->setContentsMargins(0, 0, 0, 0);
        l->addWidget(m_ffmpeg, 1);
        l->addWidget(browseFfmpeg);
        l->addWidget(detectFfmpeg);
    }

    m_ps1 = new QGroupBox(tr("Add PS1 assets from a PS1 disc image"), this);
    m_ps1->setCheckable(true);
    m_ps1->setChecked(false);
    m_ps1Image = new QLineEdit(m_ps1);
    auto* browsePs1 = new QPushButton(tr("Browse..."), m_ps1);
    m_ps1Credits = new QCheckBox(
        tr("Ending-credit data (STAFF.STF, STAFF2.STF, BIO.TIM)"), m_ps1);
    m_ps1Credits->setChecked(true);
    m_ps1Subs = new QCheckBox(
        tr("Prologue FMV subtitles (JIMAKU*.RGB -> data/jimaku*.png)"), m_ps1);
    m_ps1Subs->setChecked(true);
    m_ps1Movies = new QCheckBox(
        tr("Convert PS1 movies (STR -> MP4): STFC/STFJ and any the tree lacks"),
        m_ps1);
    m_ps1Movies->setChecked(true);
    m_ps1Replace = new QCheckBox(
        tr("Replace the tree's own movies with the PS1 versions"), m_ps1);
    m_ps1Audio = new QCheckBox(
        tr("Replace the tree's sounds, voices and music with the PS1 audio"), m_ps1);
    m_ps1AudioFormat = new QComboBox(m_ps1);
    m_ps1AudioFormat->addItem(tr("WAV (lossless, ~525 MB)"), false);
    m_ps1AudioFormat->addItem(tr("Ogg Vorbis (needs ffmpeg, ~1/5 the size)"), true);
    m_ps1AudioQuality = new QSpinBox(m_ps1);
    m_ps1AudioQuality->setRange(0, 10);
    m_ps1AudioQuality->setValue(6);
    m_ps1AudioQuality->setToolTip(tr("Vorbis quality (-q:a). 6 is transparent for "
                                     "these sources; higher is larger."));
    m_ps1AudioSfx = new QCheckBox(tr("Sound effects (VAB banks)"), m_ps1);
    m_ps1AudioSfx->setChecked(true);
    m_ps1AudioVoices = new QCheckBox(tr("Voices (CD-XA, needs a raw .bin/.cue)"), m_ps1);
    m_ps1AudioVoices->setChecked(true);
    m_ps1AudioBgm = new QCheckBox(tr("BGM (rendered from the PS1 sequences)"), m_ps1);
    m_ps1AudioBgm->setChecked(true);
    {
        auto* ps1Form = new QFormLayout(m_ps1);
        ps1Form->addRow(tr("PS1 image:"), pathRow(m_ps1Image, browsePs1));
        ps1Form->addRow(QString(), m_ps1Credits);
        ps1Form->addRow(QString(), m_ps1Subs);
        ps1Form->addRow(QString(), m_ps1Movies);
        ps1Form->addRow(QString(), m_ps1Replace);
        ps1Form->addRow(QString(), m_ps1Audio);
        auto* audioRow = new QHBoxLayout();
        audioRow->setContentsMargins(24, 0, 0, 0);
        audioRow->addWidget(m_ps1AudioSfx);
        audioRow->addWidget(m_ps1AudioVoices);
        audioRow->addWidget(m_ps1AudioBgm);
        audioRow->addStretch(1);
        ps1Form->addRow(QString(), audioRow);
        auto* formatRow = new QHBoxLayout();
        formatRow->setContentsMargins(24, 0, 0, 0);
        formatRow->addWidget(new QLabel(tr("Format:"), m_ps1));
        formatRow->addWidget(m_ps1AudioFormat);
        formatRow->addWidget(new QLabel(tr("Quality:"), m_ps1));
        formatRow->addWidget(m_ps1AudioQuality);
        formatRow->addStretch(1);
        ps1Form->addRow(QString(), formatRow);
        auto* ps1Note = new WrappedNote(
            tr("For the PS1 staff and cast rolls in OG mode "
               "([Game] Ps1EndingCredits=1), the Japanese prologue FMV "
               "subtitles ([Game] Ps1FmvSubtitles=1, read from the JPN tree) "
               "and the PS1 sound effects, voices and music, which replace the "
               "tree's own files (running with a PC source again restores "
               "them). Use a raw .bin/.cue so the movie audio and the voices "
               "are intact. With a PS1 image the PC source may be left empty "
               "to update an existing tree."),
            m_ps1);
        ps1Form->addRow(ps1Note);
    }

    auto* form = new QFormLayout();
    form->addRow(tr("Source type:"), m_sourceKind);
    form->addRow(tr("Source:"), pathRow(m_source, browseSource));
    form->addRow(tr("Target game folder:"), pathRow(m_target, browseTarget));
    form->addRow(tr("Asset type:"), m_version);
    form->addRow(QString(), m_convert);
    form->addRow(QString(), m_keepAvi);
    form->addRow(tr("ffmpeg:"), ffmpegRow);

    m_run = new RunPanel(this);

    auto* note = new WrappedNote(
        tr("Copies the release's asset folders into <target>/USA or "
           "<target>/JPN. From a disc image the folders are extracted first. "
           "The base trees are only added to, never replaced."),
        this);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(note);
    layout->addLayout(form);
    layout->addWidget(m_ps1);
    layout->addWidget(m_run, 1);

    connect(m_sourceKind, &QComboBox::currentIndexChanged, this,
            &PcAssetsTab::onSourceKindChanged);
    connect(browseSource, &QPushButton::clicked, this, &PcAssetsTab::onBrowseSource);
    connect(browsePs1, &QPushButton::clicked, this, &PcAssetsTab::onBrowsePs1);
    connect(m_ps1, &QGroupBox::toggled, this, &PcAssetsTab::syncEnabled);
    connect(m_ps1Movies, &QCheckBox::toggled, this, &PcAssetsTab::syncEnabled);
    connect(m_ps1Audio, &QCheckBox::toggled, this, &PcAssetsTab::syncEnabled);
    connect(m_ps1AudioFormat, &QComboBox::currentIndexChanged, this,
            &PcAssetsTab::syncEnabled);
    connect(browseTarget, &QPushButton::clicked, this, &PcAssetsTab::onBrowseTarget);
    connect(browseFfmpeg, &QPushButton::clicked, this, &PcAssetsTab::onBrowseFfmpeg);
    connect(detectFfmpeg, &QPushButton::clicked, this, &PcAssetsTab::onDetectFfmpeg);
    connect(m_convert, &QCheckBox::toggled, this, &PcAssetsTab::syncEnabled);

    m_run->setTaskFactory([this]() -> Worker* {
        re1::PcMigrationOptions opts;
        opts.sourcePath = m_source->text().toStdString();
        opts.sourceIsImage = m_sourceKind->currentData().toBool();
        opts.targetRoot = m_target->text().toStdString();
        opts.version =
            (re1::AssetVersion)m_version->currentData().toInt();
        opts.convertMovies = m_convert->isChecked();
        opts.keepAvi = m_keepAvi->isChecked();
        opts.ffmpegPath = m_ffmpeg->text().toStdString();
        if (m_ps1->isChecked()) {
            opts.ps1ImagePath = m_ps1Image->text().toStdString();
            opts.ps1Credits = m_ps1Credits->isChecked();
            opts.ps1FmvSubtitles = m_ps1Subs->isChecked();
            opts.ps1Movies = m_ps1Movies->isChecked();
            opts.ps1ReplaceMovies = m_ps1Replace->isChecked();
            opts.ps1Audio = m_ps1Audio->isChecked();
            opts.ps1AudioSfx = m_ps1AudioSfx->isChecked();
            opts.ps1AudioVoices = m_ps1AudioVoices->isChecked();
            opts.ps1AudioBgm = m_ps1AudioBgm->isChecked();
            opts.ps1AudioOgg = m_ps1AudioFormat->currentData().toBool();
            opts.ps1AudioOggQuality = m_ps1AudioQuality->value();
        }
        return new Worker(
            [opts](const re1::Progress& p, QString& error) {
                std::string err;
                const bool ok = re1::migratePcAssets(opts, p, &err);
                if (!ok) error = QString::fromStdString(err);
                return ok;
            });
    });

    syncEnabled();
}

void PcAssetsTab::syncEnabled() {
    const bool ps1Movies = m_ps1->isChecked() && m_ps1Movies->isChecked();
    const bool audio = m_ps1Audio->isChecked();
    const bool ogg = audio && m_ps1AudioFormat->currentData().toBool();
    m_keepAvi->setEnabled(m_convert->isChecked());
    m_ffmpeg->setEnabled(m_convert->isChecked() || ps1Movies ||
                         (m_ps1->isChecked() && ogg));
    m_ps1Replace->setEnabled(m_ps1Movies->isChecked());
    m_ps1AudioSfx->setEnabled(audio);
    m_ps1AudioVoices->setEnabled(audio);
    m_ps1AudioBgm->setEnabled(audio);
    m_ps1AudioFormat->setEnabled(audio);
    m_ps1AudioQuality->setEnabled(ogg);
}

void PcAssetsTab::onSourceKindChanged() {
    m_source->clear();
    syncEnabled();
}

void PcAssetsTab::onBrowseSource() {
    if (m_sourceKind->currentData().toBool()) {
        const QString f = QFileDialog::getOpenFileName(
            this, tr("Select a disc image"), m_source->text(),
            tr("Disc images (*.iso *.bin *.cue);;All files (*)"));
        if (!f.isEmpty()) m_source->setText(f);
    } else {
        const QString d = QFileDialog::getExistingDirectory(
            this, tr("Select the extracted asset folder"), m_source->text());
        if (!d.isEmpty()) m_source->setText(d);
    }
}

void PcAssetsTab::onBrowsePs1() {
    const QString f = QFileDialog::getOpenFileName(
        this, tr("Select a PS1 disc image"), m_ps1Image->text(),
        tr("Disc images (*.cue *.bin *.img *.iso);;All files (*)"));
    if (!f.isEmpty()) m_ps1Image->setText(f);
}

void PcAssetsTab::onBrowseTarget() {
    const QString d = QFileDialog::getExistingDirectory(
        this, tr("Select the game folder"), m_target->text());
    if (!d.isEmpty()) m_target->setText(d);
}

void PcAssetsTab::onBrowseFfmpeg() {
    const QString f = QFileDialog::getOpenFileName(
        this, tr("Select ffmpeg"), m_ffmpeg->text(),
        tr("ffmpeg (ffmpeg*.exe);;All files (*)"));
    if (!f.isEmpty()) m_ffmpeg->setText(f);
}

void PcAssetsTab::onDetectFfmpeg() {
    const QString f = QStandardPaths::findExecutable(QStringLiteral("ffmpeg"));
    if (!f.isEmpty()) {
        m_ffmpeg->setText(f);
        m_run->appendLog(tr("Found ffmpeg: %1").arg(f));
    } else {
        m_run->appendLog(tr("ffmpeg was not found on PATH."));
    }
}
