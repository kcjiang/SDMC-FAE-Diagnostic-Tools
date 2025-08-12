#include "MainWindow.h"
#include "ui_MainWindow.h"
#include "AdbManager.h"
#include "SerialPortManager.h"

#include <QDateTime>
#include <QScrollBar>
#include <QMessageBox>
#include <QFileDialog>
#include <QDir>
#include <QPixmap>
#include <QRegularExpression>
#include <QTextStream>
#include <QSerialPortInfo>
#include <QCoreApplication>
#include <QFileInfo>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow),
      serialManager(new SerialPortManager(this)),
      adbManager(new AdbManager(this))
{
    ui->setupUi(this);

    // 初始化UI
    ui->functionTabs->setCurrentIndex(0);
    ui->functionList->setCurrentRow(0);
    connect(ui->functionList, &QListWidget::currentRowChanged, this, &MainWindow::onFunctionChanged);

    ui->filterLevelCombo->addItems({"ALL", "V", "D", "I", "W", "E"});
    ui->autoScrollCheck->setChecked(true);
    ui->logTextEdit->setFont(QFont("Consolas", 10));

    // 连接按钮信号
    connect(ui->btnStartLog, &QPushButton::clicked, this, &MainWindow::startLogcat);
    connect(ui->btnStopLog, &QPushButton::clicked, this, &MainWindow::stopLogcat);
    connect(ui->btnExportLog, &QPushButton::clicked, this, &MainWindow::exportLog);
    connect(ui->btnScreenshot, &QPushButton::clicked, this, &MainWindow::captureScreenshot);

    // 串口相关连接
    connect(ui->refreshPortsBtn, &QPushButton::clicked, this, &MainWindow::refreshSerialPorts);
    connect(ui->openPortBtn, &QPushButton::clicked, this, &MainWindow::openSerialPort);
    connect(ui->closePortBtn, &QPushButton::clicked, this, &MainWindow::closeSerialPort);

    // 连接串口管理器的信号
    connect(serialManager, &SerialPortManager::dataReceived, this, &MainWindow::onSerialDataReceived);
    connect(serialManager, &SerialPortManager::portOpened, this, &MainWindow::onPortOpened);
    connect(serialManager, &SerialPortManager::portClosed, this, &MainWindow::onPortClosed);
    connect(serialManager, &SerialPortManager::errorOccurred, this, &MainWindow::onSerialError);

    // 连接ADB管理器的信号
    m_adbManager = new AdbManager(this);  // 实例化先
    connect(m_adbManager, &AdbManager::logMessage, this, &MainWindow::appendLog);     // 新增日志窗口提示信号
    connect(adbManager, &AdbManager::deviceStatusUpdated, this, [this](const QString &status, const QString &color, 
             const QString &serial, const QString &brand, const QString &model, const QString &androidVer, const QString &imagePath) {
        ui->statusLabel->setText("设备状态: " + status);
        ui->statusLabel->setStyleSheet("color:" + color);
        ui->serialLabel->setText("序列号: " + serial);
        ui->brandLabel->setText("品牌: " + brand);
        ui->modelLabel->setText("型号: " + model);
        ui->androidLabel->setText("安卓版本: " + androidVer);

        // 更新设备图片
        QPixmap pix(imagePath);
            if (!pix.isNull()) {
                ui->deviceImageLabel->setPixmap(pix.scaled(ui->deviceImageLabel->size(), Qt::KeepAspectRatio));
            } else {
                ui->deviceImageLabel->setPixmap({});
                ui->deviceImageLabel->setText("(无产品图片)");
            }
    });
    connect(adbManager, &AdbManager::screenshotCaptured, this, [this](const QString &filePath) {
        appendLog("截图已保存: " + filePath);
        showInfo("完成", "截图已保存至:\n" + filePath);
    });
    // 使用lambda解决参数不匹配问题
    connect(adbManager, &AdbManager::errorOccurred, this, [this](const QString &msg) {
        showError("ADB错误", msg);
    });

    // 初始化定时器
    logUpdateTimer = new QTimer(this);
    connect(logUpdateTimer, &QTimer::timeout, this, &MainWindow::processLogQueue);
    logUpdateTimer->start(100);

    // 初始检查设备状态
    // adbManager->checkDeviceStatus(); // 这里不需要立即检查，等用户点击ADB功能时再检查
    refreshSerialPorts();

    // 定时检查设备ADB状态
    QTimer *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, m_adbManager, &AdbManager::checkDeviceStatus);
    timer->start(3000);  // 每3秒检测一次设备状态

}

MainWindow::~MainWindow() {
    stopLogcat();
    closeSerialPort();
    delete ui;
}

void MainWindow::onFunctionChanged(int index) {
    ui->functionTabs->setCurrentIndex(index);
}

void MainWindow::refreshSerialPorts() {
    serialManager->refreshAvailablePorts();
    ui->serialPortCombo->clear();
    ui->serialPortCombo->addItems(serialManager->availablePorts());
}

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

void MainWindow::closeSerialPort() {
    serialManager->closePort();
}

void MainWindow::onSerialDataReceived(const QString &data) {
    QList<QByteArray> lines = data.toUtf8().split('\n');
    for (const QByteArray &line : lines) {
        QString msg = QString::fromUtf8(line).trimmed();
        if (!msg.isEmpty()) {
            m_logQueue.push(msg);
        }
    }
}

void MainWindow::onPortOpened(bool success, const QString &message) {
    if (success) {
        appendLog(message);
        ui->statusLabel->setText("设备状态: 串口已连接: " + serialManager->currentPortName());
    } else {
        showError("错误", message);
    }
}

void MainWindow::onPortClosed() {
    appendLog("串口已关闭");
    ui->statusLabel->setText("设备状态: 串口已关闭");
}

void MainWindow::onSerialError(const QString &error) {
    showError("串口错误", error);
}

QColor MainWindow::colorForLevel(const QString& level) {
    for (const auto &ll : LEVELS) {
        if (ll.level == level)
            return ll.color;
    }
    return Qt::black;
}

int MainWindow::levelIndex(const QString &level) {
    for (int i = 0; i < LEVELS.size(); ++i)
        if (LEVELS[i].level == level) return i;
    return 2; // 默认I
}

void MainWindow::processLogQueue() {
    QString msg;
    while (m_logQueue.pop(msg)) {
        bool showMessage = true;

        QString levelChar = "I";
        QRegularExpression re(R"(\b([VDIWE])[/\s])");
        QRegularExpressionMatch match = re.match(msg);
        if (match.hasMatch()) {
            levelChar = match.captured(1);
        }

        QString keyword = ui->filterKeywordEdit->text().trimmed();
        if (!keyword.isEmpty() && !msg.contains(keyword, Qt::CaseInsensitive))
            showMessage = false;

        QString currentFilter = ui->filterLevelCombo->currentText();
        if (currentFilter != "ALL") {
            int minLevelIndex = levelIndex(currentFilter);
            int msgLevelIndex = levelIndex(levelChar);
            if (msgLevelIndex < minLevelIndex)
                showMessage = false;
        }

        if (showMessage) {
            QTextCharFormat fmt;
            fmt.setForeground(colorForLevel(levelChar));
            ui->logTextEdit->setCurrentCharFormat(fmt);
            ui->logTextEdit->append(msg);
        }
    }

    auto sb = ui->logTextEdit->verticalScrollBar();
    bool atBottom = (sb->value() >= sb->maximum() - 3);
    if (ui->autoScrollCheck->isChecked() && atBottom) {
        sb->setValue(sb->maximum());
    }
}

void MainWindow::startLogcat() {
    adbManager->startLogcat();
}

void MainWindow::stopLogcat() {
    adbManager->stopLogcat();
}

void MainWindow::exportLog() {
    QString content = ui->logTextEdit->toPlainText().trimmed();
    if (content.isEmpty()) {
        showWarning("提示", "当前没有可导出的日志");
        return;
    }

    QString defaultName = "filtered_log_" + QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss") + ".txt";
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

void MainWindow::captureScreenshot() {
    adbManager->captureScreenshot();
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
