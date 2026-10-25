#pragma once
// Runs a blocking core migration on a worker thread and reports progress back
// to the GUI thread through queued signals.

#include "core/Types.h"

#include <QObject>
#include <QString>

#include <atomic>
#include <functional>

class Worker : public QObject {
    Q_OBJECT
public:
    using Task =
        std::function<bool(const re1::Progress&, QString& error)>;

    explicit Worker(Task task) : m_task(std::move(task)) {}

public slots:
    void run() {
        re1::Progress p;
        p.log = [this](const std::string& s) {
            emit logMessage(QString::fromStdString(s));
        };
        p.step = [this](int done, int total, const std::string& label) {
            emit progressChanged(done, total, QString::fromStdString(label));
            return !m_cancelled.load();
        };
        p.cancelled = [this] { return m_cancelled.load(); };

        QString error;
        bool ok = false;
        try {
            ok = m_task(p, error);
        } catch (const std::exception& e) {
            error = QString::fromUtf8(e.what());
            ok = false;
        } catch (...) {
            error = QStringLiteral("unknown error");
            ok = false;
        }
        emit finished(ok, error);
    }

    void requestCancel() { m_cancelled.store(true); }

signals:
    void logMessage(const QString& text);
    void progressChanged(int done, int total, const QString& label);
    void finished(bool ok, const QString& error);

private:
    Task m_task;
    std::atomic<bool> m_cancelled{false};
};
