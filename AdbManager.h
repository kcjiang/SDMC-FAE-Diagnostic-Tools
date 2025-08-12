#ifndef ADBMANAGER_H
#define ADBMANAGER_H

#include <QObject>
#include <QProcess>
#include <QFile>
#include <QAtomicInt>
#include <QQueue>
#include <QMutex>
#include <atomic>

class AdbManager : public QObject
{
    Q_OBJECT

public:
    explicit AdbManager(QObject *parent = nullptr);
    ~AdbManager();

    // ADB 路径管理
    QString getAdbPath() const;
    // void setAdbPath(const QString &path);

    // 设备管理
    void checkDeviceStatus();
    bool isDeviceConnected() const;   // 连接状态

    // 日志管理
    void startLogcat();
    void stopLogcat();
    void clearLogcat();

    // 截图管理
    void captureScreenshot();

    // 命令执行
    QString runCommand(const QString &cmd);

    // 设备信息
    QString serialNumber() const;
    QString deviceBrand() const;
    QString deviceModel() const;
    QString androidVersion() const;

signals:
    void deviceStatusUpdated(const QString &status, const QString &color,
                             const QString &serial, const QString &brand,
                             const QString &model, const QString &androidVer, const QString &imagePath);
    
    void logReceived(const QString &log);
    void screenshotCaptured(const QString &filePath);
    void errorOccurred(const QString &error);
    // AdbManager 发日志 → MainWindow::appendLog 收到 → 推入 m_logQueue → 后台 UI 定时刷新显示。
    void logMessage(const QString &msg);  // 新增定义日志窗口提示信号
    void deviceConnectionChanged(bool connected);

private slots:
    void onLogcatReadyRead();
    void onLogcatFinished(int exitCode, QProcess::ExitStatus exitStatus);

private:
    QProcess *m_logcatProcess = nullptr;
    QFile m_logFile;
    std::atomic<bool> m_stopLogFlag;
    QQueue<QString> m_logQueue;
    QMutex m_logMutex;

    QString m_adbPath;
    QString m_serialNumber;
    QString m_deviceBrand;
    QString m_deviceModel;
    QString m_androidVersion;
    bool m_deviceConnected = false;       // 设备连接状态缓存

    void processLogData(const QByteArray &data);
    QString getScreenshotTempPath() const;
};

#endif // ADBMANAGER_H