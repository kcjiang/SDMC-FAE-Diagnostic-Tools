#include "SerialPortManager.h"
#include <QDebug>

SerialPortManager::SerialPortManager(QObject *parent) : QObject(parent), serial(nullptr)
{
    serial = new QSerialPort(this);
    connect(serial, &QSerialPort::readyRead, this, &SerialPortManager::onReadyRead);
    connect(serial, &QSerialPort::errorOccurred, this, [this](QSerialPort::SerialPortError error) {
        if (error != QSerialPort::NoError) {
            emit errorOccurred(serial->errorString());
        }
    });
}

SerialPortManager::~SerialPortManager()
{
    closePort();
    delete serial;
}

void SerialPortManager::refreshAvailablePorts()
{
    portList.clear();
    const auto infos = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &info : infos) {
        portList.append(info.portName());
    }
}

bool SerialPortManager::openPort(const QString &portName, int baudRate)
{
    if (serial->isOpen()) {
        emit errorOccurred("Port is already open");
        return false;
    }

    serial->setPortName(portName);
    serial->setBaudRate(baudRate);
    serial->setDataBits(QSerialPort::Data8);
    serial->setParity(QSerialPort::NoParity);
    serial->setStopBits(QSerialPort::OneStop);
    serial->setFlowControl(QSerialPort::NoFlowControl);

    if (serial->open(QIODevice::ReadWrite)) {
        emit portOpened(true, QString("Port %1 opened successfully").arg(portName));
        return true;
    } else {
        emit errorOccurred(QString("Failed to open port: %1").arg(serial->errorString()));
        return false;
    }
}

void SerialPortManager::closePort()
{
    if (serial->isOpen()) {
        serial->close();
        emit portClosed();
    }
}

bool SerialPortManager::isPortOpen() const
{
    return serial->isOpen();
}

QStringList SerialPortManager::availablePorts() const
{
    return portList;
}

QString SerialPortManager::currentPortName() const
{
    return serial->portName();
}

void SerialPortManager::writeData(const QByteArray &data)
{
    if (serial->isOpen()) {
        serial->write(data);
    }
}

void SerialPortManager::onReadyRead()
{
    QByteArray data = serial->readAll();
    while (serial->waitForReadyRead(10)) {
        data += serial->readAll();
    }
    emit dataReceived(QString::fromUtf8(data));
}