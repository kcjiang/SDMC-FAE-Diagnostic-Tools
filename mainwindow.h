#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <QColor>
#include <QQueue>
#include <QMutex>
#include "AdbManager.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

// 日志级别颜色
struct LogLevel {
    QString level;
    QColor color;
};

static const QVector<LogLevel> LEVELS = {
    {"V", QColor(128,128,128)},  // Verbose 灰色
    {"D", QColor(0,128,0)},      // Debug   绿色
    {"I", QColor(0,0,255)},      // Info    蓝色
    {"W", QColor(255,140,0)},    // Warn    橙色
    {"E", QColor(255,0,0)}       // Error   红色
};

// 简单线程安全队列（用于日志）
class LogQueue {
public:
    void push(const QString &msg) {
        QMutexLocker locker(&m_mutex);
        m_queue.enqueue(msg);
    }
    bool pop(QString &msg) {
        QMutexLocker locker(&m_mutex);
        if (m_queue.isEmpty()) return false;
        msg = m_queue.dequeue();
        return true;
    }
private:
    QQueue<QString> m_queue;
    QMutex m_mutex;
};

// 前向声明
class AdbManager;
class SerialPortManager;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    // 左侧功能切换
    void onFunctionChanged(int index);

    // 串口逻辑
    void refreshSerialPorts();
    void openSerialPort();
    void closeSerialPort();
    void onSerialDataReceived(const QString &data);
    void onPortOpened(bool success, const QString &message);
    void onPortClosed();
    void onSerialError(const QString &error);

    // 日志相关
    void processLogQueue();
    void startLogcat();
    void stopLogcat();
    void exportLog();
    void captureScreenshot();

private:
    Ui::MainWindow *ui;
    QString currentConnection;           // 当前连接类型（ADB/串口）

    QTimer *logUpdateTimer;              // 定时更新日志

    LogQueue m_logQueue;                 // 日志队列

    SerialPortManager *serialManager;    // 串口管理对象
    AdbManager *adbManager;              // ADB管理对象
    AdbManager *m_adbManager;            // 添加成员变量

private:
    // 工具方法
    void appendLog(const QString &msg);
    void showWarning(const QString &title, const QString &msg);
    void showInfo(const QString &title, const QString &msg);
    void showError(const QString &title, const QString &msg);

    QColor colorForLevel(const QString &level);
    int levelIndex(const QString &level);
};

#endif // MAINWINDOW_H