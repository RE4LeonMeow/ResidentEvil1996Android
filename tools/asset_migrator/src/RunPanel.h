#pragma once
// Reusable run panel: a log view, a progress bar and Run/Cancel buttons, plus
// the worker-thread plumbing every tab needs.

#include <QWidget>

class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class QLabel;
class QThread;
class Worker;

class RunPanel : public QWidget {
    Q_OBJECT
public:
    explicit RunPanel(QWidget* parent = nullptr);
    ~RunPanel() override;

    void appendLog(const QString& text);
    void clearLog();

    // Start a task. `cancelHint` is shown while it runs.
    void start(const QString& runningLabel);
    void finish(bool ok, const QString& error);

    bool isRunning() const { return m_running; }

    // Owner builds the task; the panel owns the thread and lifecycle.
    void setTaskFactory(std::function<Worker*()> factory) {
        m_factory = std::move(factory);
    }

signals:
    void started();
    void stopped();

private slots:
    void onRunClicked();
    void onCancelClicked();
    void onLog(const QString& text);
    void onProgress(int done, int total, const QString& label);
    void onFinished(bool ok, const QString& error);

private:
    QPlainTextEdit* m_log = nullptr;
    QProgressBar* m_progress = nullptr;
    QPushButton* m_run = nullptr;
    QPushButton* m_cancel = nullptr;
    QLabel* m_status = nullptr;
    QThread* m_thread = nullptr;
    Worker* m_worker = nullptr;
    std::function<Worker*()> m_factory;
    bool m_running = false;
};
