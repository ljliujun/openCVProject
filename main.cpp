#include "openCVTest.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    openCVTest window;
    window.show();
    return app.exec();
}
