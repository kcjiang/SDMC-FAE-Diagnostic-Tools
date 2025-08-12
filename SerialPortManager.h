#ifndef SERIALPORTMANAGER_H
#define SERIALPORTMANAGER_H

#include <QObject>
#include <QSerialPort>
#include <QSerialPortInfo>

class SerialPortManager : public QObject
{
    Q_OBJECT

public:
    explicit SerialPortManager(QObject *parent = nullptr);
    ~SerialPortManager();

    // 公共接口
    void refreshAvailablePorts();
    bool openPort(const QString &portName, int baudRate);
    void closePort();
    bool isPortOpen() const;
    QStringList availablePorts() const;
    QString currentPortName() const;

signals:
    void dataReceived(const QString &data);
    void portOpened(bool success, const QString &message);
    void portClosed();
    void errorOccurred(const QString &error);

public slots:
    void writeData(const QByteArray &data);

private slots:
    void onReadyRead();

private:
    QSerialPort *serial;
    QStringList portList;
};

#endif // SERIALPORTMANAGER_H