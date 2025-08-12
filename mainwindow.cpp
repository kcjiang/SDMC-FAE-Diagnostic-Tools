#include "MainWindow.h"
#include "ui_MainWindow.h"
#include "SerialPortManager.h"

// 引入常用Qt模块
#include <QDateTime>
#include <QScrollBar>
#include <QMessageBox>
#include <QFileDialog>
#include <QDir>
#include <QPixmap>
#include <QThreadPool>
#include <QRegularExpression>
#include <QTextStream>
#include <QSerialPortInfo>
#include <QCoreApplication>
#include <QFileInfo>

// 构造函数：初始化主窗口
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow), serialManager(new SerialPortManager(this))
{
    // 设置 UI 界面
    ui->setupUi(this);

    // 默认选中左侧功能列表的第一项和对应功能页
    ui->functionTabs->setCurrentIndex(0);
    ui->functionList->setCurrentRow(0);
    connect(ui->functionList, &QListWidget::currentRowChanged, this, &MainWindow::onFunctionChanged);

    // 初始化日志过滤等级下拉框
    ui->filterLevelCombo->addItems({"ALL", "V", "D", "I", "W", "E"});
    ui->autoScrollCheck->setChecked(true);  // 默认启用自动滚动
    ui->logTextEdit->setFont(QFont("Consolas", 10));  // 设置字体

    // 按钮事件绑定
    connect(ui->btnStartLog, &QPushButton::clicked, this, &MainWindow::startLogcat);
    connect(ui->btnStopLog, &QPushButton::clicked, this, &MainWindow::stopLogcat);
    connect(ui->btnExportLog, &QPushButton::clicked, this, &MainWindow::exportLog);
    connect(ui->btnScreenshot, &QPushButton::clicked, this, &MainWindow::captureScreenshot);

    connect(ui->refreshPortsBtn, &QPushButton::clicked, this, &MainWindow::refreshSerialPorts);
    connect(ui->openPortBtn, &QPushButton::clicked, this, &MainWindow::openSerialPort);
    connect(ui->closePortBtn, &QPushButton::clicked, this, &MainWindow::closeSerialPort);

    // 串口管理器信号绑定
    connect(serialManager, &SerialPortManager::dataReceived, this, &MainWindow::onSerialDataReceived);
    connect(serialManager, &SerialPortManager::portOpened, this, &MainWindow::onPortOpened);
    connect(serialManager, &SerialPortManager::portClosed, this, &MainWindow::onPortClosed);
    connect(serialManager, &SerialPortManager::errorOccurred, this, &MainWindow::onSerialError);

    // 日志定时器：用于处理日志队列
    logUpdateTimer = new QTimer(this);
    connect(logUpdateTimer, &QTimer::timeout, this, &MainWindow::processLogQueue);
    logUpdateTimer->start(100); // 每100ms更新一次

    // 设备状态定时器：每3秒检测一次ADB设备
    deviceCheckTimer = new QTimer(this);
    connect(deviceCheckTimer, &QTimer::timeout, this, &MainWindow::checkDeviceStatus);
    deviceCheckTimer->start(3000);

    m_stopLogFlag.store(false); // 初始化日志停止标志
    refreshSerialPorts();       // 启动时刷新串口列表
    checkDeviceStatus();        // 检测一次ADB设备
}

// 析构函数
MainWindow::~MainWindow() {
    stopLogcat();       // 停止日志抓取
    closeSerialPort();  // 关闭串口
    delete ui;
}

// 切换功能页（左侧功能列表）
void MainWindow::onFunctionChanged(int index) {
    ui->functionTabs->setCurrentIndex(index);
}

// 获取当前ADB路径（优先使用本地adb.exe）
QString MainWindow::getAdbPath() const {
    QString localAdb = QCoreApplication::applicationDirPath() + "/adb.exe";
    if (QFileInfo::exists(localAdb))
        return localAdb;
    return "adb";
}

// 刷新串口列表
void MainWindow::refreshSerialPorts() {
    serialManager->refreshAvailablePorts();
    ui->serialPortCombo->clear();
    ui->serialPortCombo->addItems(serialManager->availablePorts());
}

// 打开串口
void MainWindow::openSerialPort() {
    if (serialManager->isPortOpen()) {
        showWarning("提示", "串口已打开");
        return;
    }

    QString portName = ui->serialPortCombo->currentText();
    if (portName.isEmpty()) {
        showWarning("提示", "请选择串口");
        return;
    }

    int baudRate = ui->baudRateCombo->currentText().toInt();
    serialManager->openPort(portName, baudRate);
}

// 关闭串口
void MainWindow::closeSerialPort() {
    serialManager->closePort();
}

// 串口接收到数据
void MainWindow::onSerialDataReceived(const QString &data) {
    QList<QByteArray> lines = data.toUtf8().split('\n');
    for (const QByteArray &line : lines) {
        QString msg = QString::fromUtf8(line).trimmed();
        if (!msg.isEmpty()) {
            m_logQueue.push(msg);
        }
    }
}

// 串口打开结果反馈
void MainWindow::onPortOpened(bool success, const QString &message) {
    if (success) {
        appendLog(message);
        ui->statusLabel->setText("设备状态: 串口已连接: " + serialManager->currentPortName());
    } else {
        showError("错误", message);
    }
}

// 串口关闭事件
void MainWindow::onPortClosed() {
    appendLog("串口已关闭");
    ui->statusLabel->setText("设备状态: 串口已关闭");
}

// 串口错误处理
void MainWindow::onSerialError(const QString &error) {
    showError("串口错误", error);
}

// 根据日志级别获取对应颜色
QColor MainWindow::colorForLevel(const QString& level) {
    for (const auto &ll : LEVELS) {
        if (ll.level == level)
            return ll.color;
    }
    return Qt::black;
}

// 将日志级别转换为整数索引
int MainWindow::levelIndex(const QString &level) {
    for (int i = 0; i < LEVELS.size(); ++i)
        if (LEVELS[i].level == level) return i;
    return 2; // 默认级别：I（Info）
}

// 执行系统命令并返回输出
QString MainWindow::runCommand(const QString &cmd) {
    QProcess proc;
#ifdef Q_OS_WIN
    proc.start("cmd", {"/c", cmd});
#else
    proc.start("/bin/sh", {"-c", cmd});
#endif
    proc.waitForFinished(3000);
    return QString::fromLocal8Bit(proc.readAllStandardOutput()).trimmed();
}

// 处理日志队列中的内容并显示到UI
void MainWindow::processLogQueue() {
    QString msg;
    while (m_logQueue.pop(msg)) {
        bool showMessage = true;

        // 获取日志级别
        QString levelChar = "I";
        QRegularExpression re(R"(\b([VDIWE])[/\s])");
        QRegularExpressionMatch match = re.match(msg);
        if (match.hasMatch()) {
            levelChar = match.captured(1);
        }

        // 关键字过滤
        QString keyword = ui->filterKeywordEdit->text().trimmed();
        if (!keyword.isEmpty() && !msg.contains(keyword, Qt::CaseInsensitive))
            showMessage = false;

        // 日志级别过滤
        QString currentFilter = ui->filterLevelCombo->currentText();
        if (currentFilter != "ALL") {
            int minLevelIndex = levelIndex(currentFilter);
            int msgLevelIndex = levelIndex(levelChar);
            if (msgLevelIndex < minLevelIndex)
                showMessage = false;
        }

        // 显示日志信息
        if (showMessage) {
            QTextCharFormat fmt;
            fmt.setForeground(colorForLevel(levelChar));
            ui->logTextEdit->setCurrentCharFormat(fmt);
            ui->logTextEdit->append(msg);
        }
    }

    // 自动滚动到底部
    auto sb = ui->logTextEdit->verticalScrollBar();
    bool atBottom = (sb->value() >= sb->maximum() - 3);
    if (ui->autoScrollCheck->isChecked() && atBottom) {
        sb->setValue(sb->maximum());
    }
}

// 检查设备状态（adb devices + 获取设备属性）
void MainWindow::checkDeviceStatus() {
    QThreadPool::globalInstance()->start([this]() {
        QString adbCmd = getAdbPath() + " devices";
        QString adbOutput = runCommand(adbCmd);
        QString status, color = "red";
        QString serial = "-", brand = "-", model = "-", androidVer = "-";

        QStringList lines = adbOutput.split('\n');
        QStringList devices;
        for (int i=1; i<lines.size(); ++i) {
            auto line = lines[i].trimmed();
            if (!line.isEmpty() && line.contains("device"))
                devices << line.split(QRegularExpression("\\s+")).first();
        }

        if (!devices.isEmpty()) {
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

        QMetaObject::invokeMethod(this, [=]() {
            ui->statusLabel->setText("设备状态: " + status);
            ui->statusLabel->setStyleSheet("color:" + color);
            ui->serialLabel->setText("序列号: " + serial);
            ui->brandLabel->setText("品牌: " + brand);
            ui->modelLabel->setText("型号: " + model);
            ui->androidLabel->setText("安卓版本: " + androidVer);

            QPixmap pix("device.png");
            if (!pix.isNull()) {
                ui->deviceImageLabel->setPixmap(pix.scaled(ui->deviceImageLabel->size(), Qt::KeepAspectRatio));
                ui->deviceImageLabel->setText("");
            } else {
                ui->deviceImageLabel->setPixmap({});
                ui->deviceImageLabel->setText("(无产品图片)");
            }
        });
    });
}

// 日志抓取开始 / 停止 / 日志导出 ****************************************************************

// 开始抓取 adb logcat 日志
void MainWindow::startLogcat() {
    // 若未检测到ADB设备，则不执行
    if (ui->statusLabel->text().contains("未检测")) {
        showWarning("提示", "未检测到ADB设备");
        return;
    }

    // 如果已有抓取进程在运行，则提示用户
    if (m_logcatProcess && m_logcatProcess->state() == QProcess::Running) {
        showWarning("提示", "日志抓取已在进行中");
        return;
    }

    // 准备保存路径
    QString saveDir = QDir::currentPath() + "/device_logs";
    QDir().mkpath(saveDir);
    QString filename = saveDir + "/log_" + QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss") + ".txt";

    appendLog("开始实时抓取日志，保存至 " + filename);

    // 重置停止标志
    m_stopLogFlag.store(false);

    // 打开日志文件
    m_logFile.setFileName(filename);
    if (!m_logFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        appendLog("日志文件打开失败");
        return;
    }

    // 若已有旧进程，先删除
    if (m_logcatProcess) {
        m_logcatProcess->deleteLater();
        m_logcatProcess = nullptr;
    }

    // 创建并启动新的 logcat 进程
    m_logcatProcess = new QProcess(this);
    connect(m_logcatProcess, &QProcess::readyReadStandardOutput, this, &MainWindow::onLogcatReadyRead);
    connect(m_logcatProcess, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished), this, &MainWindow::onLogcatFinished);

    runCommand(getAdbPath() + " logcat -c"); // 清除旧日志缓冲区
    m_logcatProcess->start(getAdbPath(), {"logcat"}); // 启动logcat
}

// 停止抓取日志
void MainWindow::stopLogcat() {
    if (m_logcatProcess && m_logcatProcess->state() == QProcess::Running) {
        m_stopLogFlag.store(true);
        m_logcatProcess->terminate();       // 请求终止
        m_logcatProcess->waitForFinished(3000); // 最多等3秒
    }

    if (m_logFile.isOpen())
        m_logFile.close();

    if (m_logcatProcess) {
        m_logcatProcess->deleteLater();
        m_logcatProcess = nullptr;
    }

    appendLog("日志抓取已停止");
    showInfo("提示", "日志抓取已停止");
}

// 导出当前UI中显示的日志（已筛选）
void MainWindow::exportLog() {
    QString content = ui->logTextEdit->toPlainText().trimmed();
    if (content.isEmpty()) {
        showWarning("提示", "当前没有可导出的日志");
        return;
    }

    // 生成默认文件名
    QString defaultName = "filtered_log_" + QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss") + ".txt";

    // 弹出保存对话框
    QString filePath = QFileDialog::getSaveFileName(this, "保存日志文件", defaultName, "Text Files (*.txt)");
    if (!filePath.isEmpty()) {
        QFile f(filePath);
        if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&f);
            out << content;
            f.close();
            showInfo("完成", "已导出筛选日志至:\n" + filePath);
        }
    }
}


// ADB 截图核心逻辑 ********************************************************************

void MainWindow::captureScreenshot() {
    // 校验设备状态
    if (ui->statusLabel->text().contains("未检测")) {
        showWarning("提示", "未检测到ADB设备");
        return;
    }

    QString saveDir = QDir::currentPath() + "/screenshots";
    QDir().mkpath(saveDir);
    QString filename = saveDir + "/screenshot_" + QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss") + ".png";

    QByteArray imageData;
    bool success = false;

    // 方法1：exec-out直接获取截图（最快）
    {
        QProcess proc;
        proc.setProcessChannelMode(QProcess::SeparateChannels);
        proc.start(getAdbPath(), {"exec-out", "screencap -p"});

        if (proc.waitForFinished(3000)) {
            imageData = proc.readAllStandardOutput();
            if (!imageData.isEmpty() && imageData.startsWith("\x89PNG")) {
                success = true;
            }
        }
    }

    // 方法2：截取临时文件 + pull
    if (!success) {
        QString tempFile = "/sdcard/screenshot_temp_" + QString::number(QDateTime::currentSecsSinceEpoch()) + ".png";

        runCommand(getAdbPath() + " shell screencap -p " + tempFile);
        QProcess proc;
        proc.start(getAdbPath(), {"pull", tempFile, filename});
        proc.waitForFinished(5000);
        runCommand(getAdbPath() + " shell rm " + tempFile);

        QPixmap pixmap;
        if (pixmap.load(filename)) {
            success = true;
        } else {
            QFile::remove(filename);
        }
    }

    // 方法3：base64转码传输
    if (!success) {
        QProcess proc;
        proc.start(getAdbPath(), {"exec-out", "screencap -p | busybox base64"});

        if (proc.waitForFinished(5000)) {
            QByteArray base64Data = proc.readAllStandardOutput().trimmed();
            imageData = QByteArray::fromBase64(base64Data);

            if (!imageData.isEmpty() && imageData.startsWith("\x89PNG")) {
                QFile f(filename);
                if (f.open(QIODevice::WriteOnly)) {
                    f.write(imageData);
                    f.close();
                    success = true;
                }
            }
        }
    }

    // 显示截图结果
    if (success) {
        QPixmap pixmap;
        if (pixmap.load(filename)) {
            appendLog("截图已保存: " + filename);
            showInfo("完成", "截图已保存至:\n" + filename);
        } else {
            appendLog("截图保存失败 - 文件损坏");
            showError("错误", "截图保存失败: 生成的文件损坏");
            QFile::remove(filename);
        }
    } else {
        appendLog("截图失败 - 所有方法均未成功");
        showError("错误", "截图失败: 所有尝试的方法均未成功");
    }
}

// 日志抓取输出处理 *******************************************************************
// logcat stdout 可读时触发
void MainWindow::onLogcatReadyRead() {
    if (m_stopLogFlag.load()) return;

    QByteArray data = m_logcatProcess->readAllStandardOutput();
    if (!data.isEmpty()) {
        m_logFile.write(data);     // 写入原始日志
        m_logFile.flush();

        // 分行并推入显示队列
        for (const auto &line : data.split('\n')) {
            QString msg = QString::fromUtf8(line).trimmed();
            if (!msg.isEmpty()) m_logQueue.push(msg);
        }
    }
}

// logcat 进程结束时触发
void MainWindow::onLogcatFinished(int, QProcess::ExitStatus) {
    if (m_logFile.isOpen()) m_logFile.close();
    appendLog("日志抓取进程结束");
}

// 工具方法 *******************************************************************

// 将信息加入日志队列（供UI异步刷新）
void MainWindow::appendLog(const QString &msg) {
    m_logQueue.push(msg);
}

// 显示警告弹窗
void MainWindow::showWarning(const QString &title, const QString &msg) {
    QMessageBox::warning(this, title, msg);
}

// 显示信息弹窗
void MainWindow::showInfo(const QString &title, const QString &msg) {
    QMessageBox::information(this, title, msg);
}

// 显示错误弹窗
void MainWindow::showError(const QString &title, const QString &msg) {
    QMessageBox::critical(this, title, msg);
}
