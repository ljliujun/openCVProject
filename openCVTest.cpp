#include "openCVTest.h"
#include "visionWidget.h"
#include <QHBoxLayout>

#ifdef _DEBUG
#pragma comment(lib,"opencv_world420d.lib")
#else
#pragma comment(lib,"opencv_world420.lib")
#endif // _DEBUG

#pragma execution_character_set("utf-8")

openCVTest::openCVTest(QWidget *parent)
    : QMainWindow(parent)
{
    ui.setupUi(this);
    initUI();
}

openCVTest::~openCVTest()
{

}

void openCVTest::initUI()
{
    VisionWidget* vistionWidget = new VisionWidget;
    setCentralWidget(vistionWidget);
}

