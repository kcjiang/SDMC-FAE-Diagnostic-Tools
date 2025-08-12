#include "AdbManager.h"
#include <QDir>
#include <QDateTime>
#include <QPixmap>
#include <QThreadPool>
#include <QRegularExpression>
#include <QCoreApplication>
#include <QFileInfo>

// 构造函数
AdbManager::AdbManager(QObject *parent)
    : QObject(parent), m_logcatProcess(nullptr), m_stopLogFlag(false), m_deviceConnected(false)
{
    // 初始化成员变量
    // setAdbPath(QCoreApplication::applicationDirPath() + "/adb.exe");  // 初始化ADB路径
    checkDeviceStatus();  // 检查设备状态
}

// 析构函数
AdbManager::~AdbManager()
{
    if (m_logcatProcess) {
        m_logcatProcess->kill();
        m_logcatProcess->deleteLater();
    }
}

// -----------------------------------------------------------------------------

// 获取当前ADB路径（优先使用本地adb.exe）
QString AdbManager::getAdbPath() const {
    QString localAdb = QCoreApplication::applicationDirPath() + "/adb.exe";
    if (QFileInfo::exists(localAdb))
        return localAdb;
    return "adb";
}

// 检查设备状态（adb devices + 获取设备属性）
void AdbManager::checkDeviceStatus() {
    QThreadPool::globalInstance()->start([this]() {
        QString adbCmd = getAdbPath() + " devices";
        QString adbOutput = runCommand(adbCmd);

        qDebug() << "adb devices output:" << adbOutput;  // <<<< 这里加上调试输出

        QString status, color = "red";
        QString serial = "-", brand = "-", model = "-", androidVer = "-";

        QStringList lines = adbOutput.split('\n');
        QStringList devices;
        for (int i=1; i<lines.size(); ++i) {
            auto line = lines[i].trimmed();
            if (!line.isEmpty() && line.contains("device"))
                devices << line.split(QRegularExpression("\\s+")).first();
        }

        // ---- 这里开始判断设备连接 ----
        bool connected = !devices.isEmpty();

        // 更新状态，并触发信号（只有状态变化才发信号）
        if (connected != m_deviceConnected) {
            m_deviceConnected = connected;
            emit deviceConnectionChanged(m_deviceConnected);
        }
        // ---- 判断结束 ----

        if (connected) {
            serial = devices[0];
            status = "已连接设备: " + serial;
            color = "green";
            brand = runCommand(getAdbPath() + " -s " + serial + " shell getprop ro.product.brand");
            model = runCommand(getAdbPath() + " -s " + serial + " shell getprop ro.product.model");
            androidVer = runCommand(getAdbPath() + " -s " + serial + " shell getprop ro.build.version.release");
        } else if (!adbOutput.contains("List of devices")) {
            status = "设备处于 fastboot 模式";
            color = "orange";
        } else {
            status = "未检测到设备";
        }

        // 匹配设备图片路径
        QString imagePath = ":/images/" + brand + "_" + model + ".png";
        if (!QFile::exists(imagePath)) imagePath = "device.png";

        emit deviceStatusUpdated(status, color, serial, brand, model, androidVer, imagePath);
    });
}

// ********************************* 日志抓取开始 / 停止 / 日志导出 *********************************
bool AdbManager::isDeviceConnected() const
{
    return m_deviceConnected;         // 这里返回状态查询
}

// 开始抓取 adb logcat 日志
void AdbManager::startLogcat()
{
    if (!isDeviceConnected()) {
        emit errorOccurred("未检测到ADB设备");
        return;
    }

    if (m_logcatProcess && m_logcatProcess->state() == QProcess::Running) {
        emit errorOccurred("日志抓取已在进行中");
        return;
    }

    // 准备保存路径
    QString saveDir = QDir::currentPath() + "/device_logs";
    QDir().mkpath(saveDir);
    QString filename = saveDir + "/log_" + QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss") + ".txt";

    emit logMessage("开始实时抓取日志，保存至 " + filename);

    // 重置停止标志
    m_stopLogFlag.store(false);

    // 打开日志文件
    m_logFile.setFileName(filename);
    if (!m_logFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        emit logMessage("日志文件打开失败");
        return;
    }

    // 若已有旧进程，先删除
    if (m_logcatProcess) {
        m_logcatProcess->deleteLater();
        m_logcatProcess = nullptr;
    }

    // 创建并启动新的 logcat 进程
    m_logcatProcess = new QProcess(this);
    connect(m_logcatProcess, &QProcess::readyReadStandardOutput, this, &AdbManager::onLogcatReadyRead);
    connect(m_logcatProcess, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished), this, &AdbManager::onLogcatFinished);

    runCommand(getAdbPath() + " logcat -c"); // 清除旧日志缓冲区
    m_logcatProcess->start(getAdbPath(), {"logcat"}); // 启动logcat
}

// 停止抓取
void AdbManager::stopLogcat()
{
    if (m_logcatProcess && m_logcatProcess->state() == QProcess::Running) {
        m_stopLogFlag.store(true);
        m_logcatProcess->terminate();
        m_logcatProcess->waitForFinished(3000);
    }

    if (m_logFile.isOpen()) {
        m_logFile.close();
    }
}

void AdbManager::clearLogcat()
{
    if (isDeviceConnected()) {
        runCommand(getAdbPath() + " logcat -c");      // 修改m_adbPath为getAdbPath()
    }
}

// ********************************* 截图功能（无法捕获-尚未修复） *********************************
void AdbManager::captureScreenshot()
{
    if (!isDeviceConnected()) {
        emit errorOccurred("未检测到ADB设备");
        return;
    }

    QThreadPool::globalInstance()->start([this]() {
        QString saveDir = QDir::currentPath() + "/screenshots";
        QDir().mkpath(saveDir);
        QString filename = saveDir + "/screenshot_" + QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss") + ".png";

        // 尝试直接截图
        QProcess proc;
        proc.setProcessChannelMode(QProcess::SeparateChannels);
        proc.start(getAdbPath(), {"exec-out", "screencap -p"});       // 修改m_adbPath为getAdbPath()
        
        bool success = false;
        if (proc.waitForFinished(3000)) {
            QByteArray imageData = proc.readAllStandardOutput();
            if (!imageData.isEmpty() && imageData.startsWith("\x89PNG")) {
                QFile f(filename);
                if (f.open(QIODevice::WriteOnly)) {
                    f.write(imageData);
                    f.close();
                    success = true;
                }
            }
        }

        if (success) {
            emit screenshotCaptured(filename);
        } else {
            emit errorOccurred("截图失败");
        }
    });
}
// ********************************************* END *********************************************

QString AdbManager::runCommand(const QString &cmd)
{
    QProcess proc;
#ifdef Q_OS_WIN
    proc.start("cmd", {"/c", cmd});
#else
    proc.start("/bin/sh", {"-c", cmd});
#endif
    proc.waitForFinished(3000);
    return QString::fromLocal8Bit(proc.readAllStandardOutput()).trimmed();
}

void AdbManager::onLogcatReadyRead()
{
    if (m_stopLogFlag.load()) return;

    QByteArray data = m_logcatProcess->readAllStandardOutput();
    if (!data.isEmpty()) {
        m_logFile.write(data);
        m_logFile.flush();
        processLogData(data);
    }
}

void AdbManager::onLogcatFinished(int, QProcess::ExitStatus)
{
    if (m_logFile.isOpen()) {
        m_logFile.close();
    }
}

void AdbManager::processLogData(const QByteArray &data)
{
    QMutexLocker locker(&m_logMutex);
    for (const auto &line : data.split('\n')) {
        QString msg = QString::fromUtf8(line).trimmed();
        if (!msg.isEmpty()) {
            emit logReceived(msg);
        }
    }
}

QString AdbManager::serialNumber() const
{
    return m_serialNumber;
}

QString AdbManager::deviceBrand() const
{
    return m_deviceBrand;
}

QString AdbManager::deviceModel() const
{
    return m_deviceModel;
}

QString AdbManager::androidVersion() const
{
    return m_androidVersion;
}

QString AdbManager::getScreenshotTempPath() const
{
    return "/sdcard/screenshot_temp_" + QString::number(QDateTime::currentSecsSinceEpoch()) + ".png";
}