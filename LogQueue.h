#ifndef LOGQUEUE_H
#define LOGQUEUE_H

#include <QString>
#include <queue>
#include <QMutex>

class LogQueue {
public:
    void push(const QString &msg) {
        QMutexLocker locker(&m_mutex);
        m_queue.push(msg);
    }

    bool pop(QString &msg) {
        QMutexLocker locker(&m_mutex);
        if (m_queue.empty()) return false;
        msg = m_queue.front();
        m_queue.pop();
        return true;
    }

private:
    std::queue<QString> m_queue;
    QMutex m_mutex;
};

#endif // LOGQUEUE_H
