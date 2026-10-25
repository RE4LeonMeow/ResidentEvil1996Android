#include "RunPanel.h"

#include "Worker.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QThread>
#include <QVBoxLayout>

RunPanel::RunPanel(QWidget* parent) : QWidget(parent) {
    m_log = new QPlainTextEdit(this);
    m_log->setReadOnly(true);
    m_log->setMinimumHeight(160);
    m_log->setLineWrapMode(QPlainTextEdit::NoWrap);

    m_progress = new QProgressBar(this);
    m_progress->setRange(0, 100);
    m_progress->setValue(0);
    m_progress->setTextVisible(true);

    m_status = new QLabel(tr("Ready."), this);

    m_run = new QPushButton(tr("Start"), this);
    m_cancel = new QPushButton(tr("Cancel"), this);
    m_cancel->setEnabled(false);

    auto* buttons = new QHBoxLayout();
    buttons->addWidget(m_run);
    buttons->addWidget(m_cancel);
    buttons->addStretch(1);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(buttons);
    layout->addWidget(m_progress);
    layout->addWidget(m_status);
    layout->addWidget(m_log, 1);

    connect(m_run, &QPushButton::clicked, this, &RunPanel::onRunClicked);
    connect(m_cancel, &QPushButton::clicked, this, &RunPanel::onCancelClicked);
}

RunPanel::~RunPanel() {
    if (m_worker) m_worker->requestCancel();
    if (m_thread) {
        m_thread->quit();
        m_thread->wait(5000);
    }
}

void RunPanel::appendLog(const QString& text) {
    m_log->appendPlainText(text);
}

void RunPanel::clearLog() { m_log->clear(); }

void RunPanel::onRunClicked() {
    if (m_running || !m_factory) return;
    clearLog();
    m_progress->setRange(0, 0);  // busy until the first progress report
    start(tr("Running..."));
}

void RunPanel::start(const QString& runningLabel) {
    m_running = true;
    m_run->setEnabled(false);
    m_cancel->setEnabled(true);
    m_status->setText(runningLabel);
    emit started();

    m_thread = new QThread(this);
    m_worker = m_factory();
    m_worker->moveToThread(m_thread);
    connect(m_thread, &QThread::started, m_worker, &Worker::run);
    connect(m_worker, &Worker::logMessage, this, &RunPanel::onLog);
    connect(m_worker, &Worker::progressChanged, this, &RunPanel::onProgress);
    connect(m_worker, &Worker::finished, this, &RunPanel::onFinished);
    connect(m_thread, &QThread::finished, m_worker, &QObject::deleteLater);
    m_thread->start();
}

void RunPanel::finish(bool ok, const QString& error) {
    m_running = false;
    m_run->setEnabled(true);
    m_cancel->setEnabled(false);
    m_progress->setRange(0, 100);
    m_progress->setValue(ok ? 100 : 0);
    if (ok) {
        m_status->setText(tr("Done."));
    } else {
        m_status->setText(tr("Failed: %1").arg(error));
        appendLog(tr("ERROR: %1").arg(error));
    }
    if (m_thread) {
        m_thread->quit();
        m_thread->wait(5000);
        m_thread->deleteLater();
        m_thread = nullptr;
        m_worker = nullptr;
    }
    emit stopped();
}

void RunPanel::onCancelClicked() {
    if (m_worker) m_worker->requestCancel();
    m_cancel->setEnabled(false);
    m_status->setText(tr("Cancelling..."));
}

void RunPanel::onLog(const QString& text) { appendLog(text); }

void RunPanel::onProgress(int done, int total, const QString& label) {
    if (total > 0) {
        m_progress->setRange(0, total);
        m_progress->setValue(done);
        m_progress->setFormat(QStringLiteral("%1/%2").arg(done).arg(total));
    } else {
        m_progress->setRange(0, 0);
    }
    if (!label.isEmpty()) m_status->setText(label);
}

void RunPanel::onFinished(bool ok, const QString& error) { finish(ok, error); }
