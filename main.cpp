#include <QApplication>
#include <QIcon>
#include "MainWindow.h"

int main(int argc, char *argv[]) {
    QApplication a(argc, argv);
    a.setWindowIcon(QIcon(":/icons/MyApp.ico"));   // 设置应用程序图标（任务栏和窗口标题栏）
    
    MainWindow w;
    w.show();
    
    return a.exec();
}
