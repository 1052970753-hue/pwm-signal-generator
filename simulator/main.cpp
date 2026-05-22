#include <QApplication>
#include "mainwindow.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("PWM Signal Generator");
    app.setApplicationVersion("1.0");

    MainWindow w;
    w.setWindowTitle("PWM 信号发生器模拟器");
    w.resize(550, 420);
    w.show();

    return app.exec();
}
